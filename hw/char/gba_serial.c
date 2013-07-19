#include "hw/sysbus.h"


typedef struct gba_serial_state {
    SysBusDevice busdev;
    MemoryRegion iomem;
    qemu_irq irq;
} gba_serial_state;


static uint64_t gba_serial_read(void *opaque, hwaddr offset, unsigned size)
{
    printf("gba_serial_read: Bad register offset 0x%x\n", (int)offset);
    return 0;
}

static void gba_serial_write(void *opaque, hwaddr offset, uint64_t value,
                            unsigned size)
{
    printf("gba_serial_write: Bad register offset 0x%x (tried to write 0x%0*"
           PRIx64 ")\n", (int)offset, size * 2, value);
}


static const MemoryRegionOps gba_serial_ops = {
    .read = gba_serial_read,
    .write = gba_serial_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};


static int gba_serial_init(SysBusDevice *dev)
{
    gba_serial_state *s = FROM_SYSBUS(gba_serial_state, dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &gba_serial_ops, s, "gba-serial",
                          0x00000004);
    sysbus_init_mmio(dev, &s->iomem);

    sysbus_init_irq(dev, &s->irq);

    return 0;
}


static void gba_serial_class_init(ObjectClass *klass, void *data)
{
    SysBusDeviceClass *sdc = SYS_BUS_DEVICE_CLASS(klass);

    sdc->init = gba_serial_init;
}

static const TypeInfo gba_serial_info = {
    .name          = "gba_serial",
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(gba_serial_state),
    .class_init    = gba_serial_class_init,
};

static void gba_serial_register_types(void)
{
    type_register_static(&gba_serial_info);
}

type_init(gba_serial_register_types);
