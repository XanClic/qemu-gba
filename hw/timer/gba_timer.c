#include "hw/sysbus.h"


typedef struct gba_timer_state {
    SysBusDevice busdev;
    MemoryRegion iomem;
    qemu_irq irq[4];
} gba_timer_state;


static uint64_t gba_timer_read(void *opaque, hwaddr offset, unsigned size)
{
    printf("gba_timer_read: Bad register offset 0x%x\n", (int)offset);
    return 0;
}

static void gba_timer_write(void *opaque, hwaddr offset, uint64_t value,
                            unsigned size)
{
    printf("gba_timer_write: Bad register offset 0x%x (tried to write 0x%0*"
           PRIx64 ")\n", (int)offset, size * 2, value);
}


static const MemoryRegionOps gba_timer_ops = {
    .read = gba_timer_read,
    .write = gba_timer_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};


static int gba_timer_init(SysBusDevice *dev)
{
    gba_timer_state *s = FROM_SYSBUS(gba_timer_state, dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &gba_timer_ops, s, "gba-timer",
                          0x00000004);
    sysbus_init_mmio(dev, &s->iomem);

    sysbus_init_irq(dev, &s->irq[0]);
    sysbus_init_irq(dev, &s->irq[1]);
    sysbus_init_irq(dev, &s->irq[2]);
    sysbus_init_irq(dev, &s->irq[3]);

    return 0;
}


static void gba_timer_class_init(ObjectClass *klass, void *data)
{
    SysBusDeviceClass *sdc = SYS_BUS_DEVICE_CLASS(klass);

    sdc->init = gba_timer_init;
}

static const TypeInfo gba_timer_info = {
    .name          = "gba_timer",
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(gba_timer_state),
    .class_init    = gba_timer_class_init,
};

static void gba_timer_register_types(void)
{
    type_register_static(&gba_timer_info);
}

type_init(gba_timer_register_types);
