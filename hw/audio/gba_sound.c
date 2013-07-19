#include "hw/sysbus.h"


typedef struct gba_sound_state {
    SysBusDevice busdev;
    MemoryRegion iomem;
    uint8_t io_state[0x50];
} gba_sound_state;


static uint64_t gba_sound_read(void *opaque, hwaddr offset, unsigned size)
{
    gba_sound_state *s = (gba_sound_state *)opaque;

    uint64_t val = 0;

    unsigned i;
    for (i = 0; i < size; i++) {
        val |= (uint64_t)s->io_state[offset++] << (i * 8);
    }

    return val;
}


static void gba_sound_write(void *opaque, hwaddr offset, uint64_t value,
                            unsigned size)
{
    gba_sound_state *s = (gba_sound_state *)opaque;

    unsigned i;
    for (i = 0; i < size; i++) {
        s->io_state[offset++] = value & 0xff;
        value >>= 8;
    }
}


static const MemoryRegionOps gba_sound_ops = {
    .read = gba_sound_read,
    .write = gba_sound_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static int gba_sound_init(SysBusDevice *dev)
{
    gba_sound_state *s = FROM_SYSBUS(gba_sound_state, dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &gba_sound_ops, s, "gba-sound",
                          0x00000050);
    sysbus_init_mmio(dev, &s->iomem);

    return 0;
}


static void gba_sound_class_init(ObjectClass *klass, void *data)
{
    SysBusDeviceClass *sdc = SYS_BUS_DEVICE_CLASS(klass);

    sdc->init = gba_sound_init;
}

static const TypeInfo gba_sound_info = {
    .name          = "gba_sound",
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(gba_sound_state),
    .class_init    = gba_sound_class_init,
};

static void gba_sound_register_types(void)
{
    type_register_static(&gba_sound_info);
}

type_init(gba_sound_register_types);
