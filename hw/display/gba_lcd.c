#include "hw/sysbus.h"
#include "ui/console.h"
#include "ui/pixel_ops.h"
#include "qemu/timer.h"


typedef struct gba_lcd_state {
    SysBusDevice busdev;
    MemoryRegion iomem;
    QemuConsole *con;
    QEMUTimer *timer;
    bool invalidate;
    bool hblank;
    int ly, lyc;
    bool irq_vb_en, irq_hb_en, irq_vm_en;
    int bg_mode;
    int bgm_45_frame;
    bool hb_intvl_free;
    bool obj_vram_map_2d;
    bool forced_blank;
    bool display_bg[4], display_obj, display_wnd[2], display_ownd;
    qemu_irq irq_vb, irq_hb, irq_vm;
} gba_lcd_state;


static void gba_lcd_update_current_line(gba_lcd_state *s)
{
    if (s->ly >= 160) {
        return;
    }

    DisplaySurface *sfc = qemu_console_surface(s->con);

    if (is_buffer_shared(sfc)) {
        return;
    }

    if ((surface_bits_per_pixel(sfc) != 32) || is_surface_bgr(sfc)) {
        fprintf(stderr, "Unknown surface format (%i bpp %s)\n",
                surface_bits_per_pixel(sfc),
                is_surface_bgr(sfc) ? "BGR" : "RGB");
        exit(1);
    }

    size_t stride = surface_stride(sfc);
    uint8_t *data = (uint8_t *)surface_data(sfc) + stride * s->ly;

    if (s->forced_blank) {
        memset(data, 255, surface_stride(sfc));
    } else {
        memset(data, 0, surface_stride(sfc));
    }


    if (s->ly == 159) {
        dpy_gfx_update(s->con, 0, 0, 240, 160);
    }
}


static void gba_lcd_update_display(void *opaque)
{
    gba_lcd_state *s = (gba_lcd_state *)opaque;

    s->invalidate = false;
}

static void gba_lcd_invalidate_display(void *opaque)
{
    gba_lcd_state *s = (gba_lcd_state *)opaque;

    s->invalidate = true;

    qemu_console_resize(s->con, 240, 160);
}


#define CHECK_WIDTH_MAX(expected) \
    if (size > expected) { \
        printf("%s: Bad access size %u (on register 0x%x)\n", \
               __func__, size, (int)offset); \
    }

static uint64_t gba_lcd_read(void *opaque, hwaddr offset, unsigned size)
{
    gba_lcd_state *s = (gba_lcd_state *)opaque;

    switch (offset)
    {
        case 0x00: // DISPCNT
            CHECK_WIDTH_MAX(4);
            return s->bg_mode
                 | (s->bgm_45_frame     <<  4)
                 | (s->hb_intvl_free    <<  5)
                 | (!s->obj_vram_map_2d <<  6)
                 | (s->forced_blank     <<  7)
                 | (s->display_bg[0]    <<  8)
                 | (s->display_bg[1]    <<  9)
                 | (s->display_bg[2]    << 10)
                 | (s->display_bg[3]    << 11)
                 | (s->display_obj      << 12)
                 | (s->display_wnd[0]   << 13)
                 | (s->display_wnd[1]   << 14)
                 | (s->display_ownd     << 15);

        case 0x04: // DISPSTAT
            CHECK_WIDTH_MAX(2);
            return ((int)((s->ly >= 160) && (s->ly <= 226))) // V-Blank
                 | ((int)s->hblank          << 1)            // H-Blank
                 | ((int)(s->ly == s->lyc)  << 2)            // V-Counter match
                 | ((int)s->irq_vb_en       << 3)            // VB IRQ enable
                 | ((int)s->irq_hb_en       << 4)            // HB IRQ enable
                 | ((int)s->irq_vm_en       << 5)            // VCM IRQ enable
                 | (s->lyc                  << 8);           // V-Counter

        case 0x06: // VCOUNT
            CHECK_WIDTH_MAX(2);
            return s->ly; // Current scan line

        default:
            printf("gba_lcd_read: Bad register offset 0x%x\n", (int)offset);
            return 0;
    }
}

static void gba_lcd_write(void *opaque, hwaddr offset, uint64_t value,
                          unsigned size)
{
    gba_lcd_state *s = (gba_lcd_state *)opaque;

    switch (offset)
    {
        case 0x00: // DISPCNT
            CHECK_WIDTH_MAX(4);
            s->bg_mode          =    value        & 7;
            s->bgm_45_frame     =   (value >>  4) & 1;
            s->hb_intvl_free    =   (value >>  5) & 1;
            s->obj_vram_map_2d  = !((value >>  6) & 1);
            s->forced_blank     =   (value >>  7) & 1;
            s->display_bg[0]    =   (value >>  8) & 1;
            s->display_bg[1]    =   (value >>  9) & 1;
            s->display_bg[2]    =   (value >> 10) & 1;
            s->display_bg[3]    =   (value >> 11) & 1;
            s->display_obj      =   (value >> 12) & 1;
            s->display_wnd[0]   =   (value >> 13) & 1;
            s->display_wnd[1]   =   (value >> 14) & 1;
            s->display_ownd     =   (value >> 15) & 1;
            break;

        case 0x04: // DISPSTAT
            CHECK_WIDTH_MAX(4);
            s->irq_vb_en = (value >> 3) & 1;
            s->irq_hb_en = (value >> 4) & 1;
            s->irq_vm_en = (value >> 5) & 1;
            if (size == 1) {
                break;
            }
            value >>= 8;
            size--;

        case 0x05: // DISPSTAT.LYC
            CHECK_WIDTH_MAX(3);
            s->lyc = value;

        case 0x06: // VCOUNT
            break;

        default:
            printf("gba_lcd_write: Bad register offset 0x%x\n", (int)offset);
            return;
    }
}


static void gba_lcd_timer(void *opaque)
{
    gba_lcd_state *s = (gba_lcd_state *)opaque;

    if (s->hblank) {
        // Scan line "done"
        if (++s->ly > 227) {
            s->ly = 0;
        }

        qemu_set_irq(s->irq_vb, s->irq_vb_en && (s->ly >= 160));
        qemu_set_irq(s->irq_vm, s->irq_vm_en && (s->ly == s->lyc));
    }

    s->hblank = !s->hblank;
    qemu_set_irq(s->irq_hb, s->irq_hb_en && s->hblank);

    if (!s->hblank) {
        gba_lcd_update_current_line(s);
    }


    static uint64_t next_call = 0;

    if (!next_call) {
        next_call = qemu_get_clock_ns(vm_clock);
    }

    next_call += (s->hblank ? 16212 : 57221);

    qemu_mod_timer_ns(s->timer, next_call);
}


static const MemoryRegionOps gba_lcd_ops = {
    .read = gba_lcd_read,
    .write = gba_lcd_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static const GraphicHwOps gba_lcd_gfx_ops = {
    .invalidate = gba_lcd_invalidate_display,
    .gfx_update = gba_lcd_update_display,
};

static int gba_lcd_init(SysBusDevice *dev)
{
    gba_lcd_state *s = FROM_SYSBUS(gba_lcd_state, dev);


    memory_region_init_io(&s->iomem, OBJECT(s), &gba_lcd_ops, s, "gba_lcd",
                          0x00000060);
    sysbus_init_mmio(dev, &s->iomem);

    sysbus_init_irq(dev, &s->irq_vb);
    sysbus_init_irq(dev, &s->irq_hb);
    sysbus_init_irq(dev, &s->irq_vm);

    s->con = graphic_console_init(DEVICE(dev), &gba_lcd_gfx_ops, s);
    qemu_console_resize(s->con, 240, 160);

    s->timer = qemu_new_timer_ns(vm_clock, gba_lcd_timer, s);
    gba_lcd_timer(s);

    return 0;
}


static void gba_lcd_class_init(ObjectClass *klass, void *data)
{
    SysBusDeviceClass *sdc = SYS_BUS_DEVICE_CLASS(klass);

    sdc->init = gba_lcd_init;
}

static const TypeInfo gba_lcd_info = {
    .name          = "gba_lcd",
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(gba_lcd_state),
    .class_init    = gba_lcd_class_init,
};

static void gba_lcd_register_types(void)
{
    type_register_static(&gba_lcd_info);
}

type_init(gba_lcd_register_types);
