#include "hw/sysbus.h"


typedef struct gba_input_state {
    SysBusDevice busdev;
    MemoryRegion iomem;
    qemu_irq irq;
} gba_input_state;


static uint64_t gba_input_read(void *opaque, hwaddr offset, unsigned size)
{
    printf("gba_input_read: Bad register offset 0x%x\n", (int)offset);
    return 0;
}

static void gba_input_write(void *opaque, hwaddr offset, uint64_t value,
                            unsigned size)
{
    printf("gba_input_write: Bad register offset 0x%x (tried to write 0x%0*"
           PRIx64 ")\n", (int)offset, size * 2, value);
}


static const MemoryRegionOps gba_input_ops = {
    .read = gba_input_read,
    .write = gba_input_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};


static int gba_input_init(SysBusDevice *dev)
{
    gba_input_state *s = FROM_SYSBUS(gba_input_state, dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &gba_input_ops, s, "gba-input",
                          0x00000004);
    sysbus_init_mmio(dev, &s->iomem);

    sysbus_init_irq(dev, &s->irq);

    return 0;
}


static void gba_input_class_init(ObjectClass *klass, void *data)
{
    SysBusDeviceClass *sdc = SYS_BUS_DEVICE_CLASS(klass);

    sdc->init = gba_input_init;
}

static const TypeInfo gba_input_info = {
    .name          = "gba_input",
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(gba_input_state),
    .class_init    = gba_input_class_init,
};

static void gba_input_register_types(void)
{
    type_register_static(&gba_input_info);
}

type_init(gba_input_register_types);
