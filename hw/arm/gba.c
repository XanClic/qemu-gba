#include "hw/hw.h"
#include "hw/boards.h"
#include "hw/loader.h"
#include "hw/sysbus.h"
#include "hw/arm/arm.h"
#include "exec/address-spaces.h"
#include "sysemu/sysemu.h"


typedef struct gba_pic_state {
    SysBusDevice busdev;
    MemoryRegion iomem;
    bool master;
    uint32_t level;
    uint32_t irq_enabled;
    qemu_irq parent_irq;
} gba_pic_state;


static void gba_pic_update(gba_pic_state *s)
{
    qemu_set_irq(s->parent_irq, s->master && (s->level & s->irq_enabled));
}


static void gba_pic_set_irq(void *opaque, int irq, int level)
{
    gba_pic_state *s = (gba_pic_state *)opaque;

    if (level) {
        s->level |=   1 << irq;
    } else {
        s->level &= ~(1 << irq);
    }

    gba_pic_update(s);
}


#define CHECK_WIDTH_MAX(expected) \
    if (size > expected) { \
        printf("%s: Bad access size %u (on register 0x%x)\n", \
               __func__, size, (int)offset); \
    }

static uint64_t gba_pic_read(void *opaque, hwaddr offset, unsigned size)
{
    gba_pic_state *s = (gba_pic_state *)opaque;

    switch (offset) {
        case 0: // IE
            CHECK_WIDTH_MAX(4);
            if (size == 2) {
                return s->irq_enabled;
            } else {
                return s->irq_enabled | (s->level << 16);
            }
        case 2: // IF
            CHECK_WIDTH_MAX(2);
            return s->level;
        case 8: // IME
            CHECK_WIDTH_MAX(4);
            return !s->master;

        default:
            printf("gba_pic_read: Bad register offset 0x%x\n", (int)offset);
            return 0;
    }
}


static void gba_pic_write(void *opaque, hwaddr offset, uint64_t value,
                          unsigned size)
{
    gba_pic_state *s = (gba_pic_state *)opaque;

    switch (offset) {
        case 0: // IE
            CHECK_WIDTH_MAX(4);
            s->irq_enabled = value & 0xffff;
            if (size < 4) {
                break;
            }
            value >>= 16;
            size -= 2;

        case 2: // IF:
            CHECK_WIDTH_MAX(2);
            s->level &= ~value;
            break;
        case 8: // IME
            CHECK_WIDTH_MAX(4);
            s->master = !(value & 1);
            break;

        default:
            printf("gba_pic_write: Bad register offset 0x%x\n", (int)offset);
            return;
    }

    gba_pic_update(s);
}


static const MemoryRegionOps gba_pic_ops = {
    .read = gba_pic_read,
    .write = gba_pic_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static int gba_pic_init(SysBusDevice *dev)
{
    gba_pic_state *s = FROM_SYSBUS(gba_pic_state, dev);

    qdev_init_gpio_in(&dev->qdev, gba_pic_set_irq, 16);
    sysbus_init_irq(dev, &s->parent_irq);
    memory_region_init_io(&s->iomem, OBJECT(s), &gba_pic_ops, s, "gba-pic",
                          0x00000100);
    sysbus_init_mmio(dev, &s->iomem);

    s->master      = false;
    s->level       = 0;
    s->irq_enabled = 0;

    return 0;
}


static void gba_map_mirrored(MemoryRegion *sys_as, MemoryRegion *mreg,
                             hwaddr start, hwaddr end, uint64_t size,
                             uint64_t skips)
{
    memory_region_add_subregion(sys_as, start, mreg);

    /*
     * Fix mal jemand QEMU. Bittedanke.
    hwaddr addr;
    for (addr = start + size; addr < end; addr += skips)
    {
        MemoryRegion *alias = g_new(MemoryRegion, 1);
        memory_region_init_alias(alias, NULL, mreg->name, mreg, 0, size);
        memory_region_add_subregion(sys_as, addr, alias);
    }
     */

    if (end > start + size) {
        MemoryRegion *final_alias = g_new(MemoryRegion, 1);
        memory_region_init_alias(final_alias, NULL, mreg->name, mreg, 0, size);
        memory_region_add_subregion(sys_as, end - skips, final_alias);
    }
}


static MemoryRegion *gba_create_ram(MemoryRegion *sys_as, const char *name,
                                    hwaddr start, hwaddr end, uint64_t size,
                                    uint64_t skips)
{
    MemoryRegion *mreg = g_new(MemoryRegion, 1);
    memory_region_init_ram(mreg, NULL, name, size);

    gba_map_mirrored(sys_as, mreg, start, end, size, skips);

    return mreg;
}


static void gba_init(QEMUMachineInitArgs *args)
{
    const char *cpu_model = args->cpu_model;

    if (!cpu_model) {
        cpu_model = "arm946";
    }

    ARMCPU *cpu = cpu_arm_init(cpu_model);
    if (!cpu) {
        fprintf(stderr, "Unable to find CPU definition\n");
        exit(1);
    }


    MemoryRegion *sys_as = get_system_memory();


    MemoryRegion *bios_rom = gba_create_ram(sys_as, "gba.bios",
                                            0x00000000, 0x00004000,
                                            0x00004000, 0x00004000);
    memory_region_set_readonly(bios_rom, true);

    gba_create_ram(sys_as, "gba.ext_wram", 0x02000000, 0x03000000,
                                           0x00040000, 0x00040000);

    gba_create_ram(sys_as, "gba.int_wram", 0x03000000, 0x04000000,
                                           0x00008000, 0x00008000);

    gba_create_ram(sys_as, "gba.bg/obj_palette_ram", 0x05000000, 0x06000000,
                                                     0x00000400, 0x00000400);

    gba_create_ram(sys_as, "gba.vram", 0x06000000, 0x07000000,
                                       0x00018000, 0x00020000);

    gba_create_ram(sys_as, "gba.oam", 0x07000000, 0x08000000,
                                      0x00000400, 0x00000400);

    MemoryRegion *cart = gba_create_ram(sys_as, "gba.cart",
                                        0x08000000, 0x0e000000,
                                        0x02000000, 0x02000000);
    memory_region_set_readonly(cart, true);

    gba_create_ram(sys_as, "gba.sram", 0x0e000000, 0x10000000,
                                       0x00010000, 0x00010000);


    qemu_irq *cpu_pic = arm_pic_init_cpu(cpu);
    qemu_irq pic[16];

    DeviceState *dev = sysbus_create_simple("gba_pic", 0x04000200, cpu_pic[ARM_PIC_CPU_IRQ]);

    int i;
    for (i = 0; i < 16; i++) {
        pic[i] = qdev_get_gpio_in(dev, i);
    }

    sysbus_create_varargs("gba_sound", 0x04000060, NULL);
    sysbus_create_varargs("gba_lcd", 0x04000000, pic[0], pic[1], pic[2], NULL);


    if (load_image_targphys(bios_name, 0x00000000, 0x00004000) < 0) {
        fprintf(stderr, "Unable to load BIOS (-bios is mandatory for GBA).\n");
        exit(1);
    }

    if (load_image_targphys(args->kernel_filename, 0x08000000, 0x02000000) < 0)
    {
        fprintf(stderr, "Unable to load ROM file (use -kernel).\n");
        exit(1);
    }
}


static QEMUMachine gba_machine = {
    .name = "gba",
    .desc = "Nintendo Game Boy Advance",
    .init = gba_init,
    DEFAULT_MACHINE_OPTIONS,
};

static void gba_machine_init(void)
{
    qemu_register_machine(&gba_machine);
}

machine_init(gba_machine_init);


static void gba_pic_class_init(ObjectClass *klass, void *data)
{
    SysBusDeviceClass *sdc = SYS_BUS_DEVICE_CLASS(klass);

    sdc->init = gba_pic_init;
}

static const TypeInfo gba_pic_info = {
    .name          = "gba_pic",
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(gba_pic_state),
    .class_init    = gba_pic_class_init,
};

static void gba_register_types(void)
{
    type_register_static(&gba_pic_info);
}

type_init(gba_register_types);
