#include "qemu/osdep.h"
#include "qapi/error.h"
#include "hw/hw.h"
#include "hw/loader.h"
#include "trace.h"
#include "ui/console.h"
#include "ui/vnc.h"
#include "hw/pci/pci.h"
#include "sysemu/kvm.h"

#include "vga_int.h"
#include "vgpu.h"

/**
 * BAR0 registers I/O handler.
 * Helper functions are moved to accel/kvm/kvm-all.c
 */
static uint64_t vgpu_reg_read(void *opaque, hwaddr addr, unsigned size)
{
    DEBUG_PRINT("Reg read at " TARGET_FMT_plx "\n", addr);

    switch (addr) {
        case REG_VM_ID:
            return (uint64_t)kvm_get_vm_id(kvm_state);

        case REG_ZERO_COPY:
            return (ava_get_zcopy_fd(kvm_state) > 0);

        case REG_ZERO_COPY_PHYS:
            return (uint64_t)(ava_get_zcopy_phys(kvm_state) & ((1UL << 32) - 1));

        case REG_ZERO_COPY_PHYS_HIGH:
            return (uint64_t)((ava_get_zcopy_phys(kvm_state) >> 32) & ((1UL << 32) - 1));

        default:
            fprintf(stderr, "read from unknown register %lx\n", addr);
    }

    return ~0;
}

static void vgpu_reg_write(void *opaque, hwaddr addr, uint64_t data,
        unsigned size)
{
    data &= 0xffffffff;
    switch (addr) {
        case REG_MOD_INIT:
            DEBUG_PRINT("vgpu module init\n");
            kvm_notify_vm_reset(kvm_state);
            break;

        case REG_MOD_EXIT:
            DEBUG_PRINT("vgpu module exit\n");
            kvm_notify_vm_exit();
            break;

        default:
            fprintf(stderr, "write 0x%lx to unknown register 0x%lx\n", data, addr);
    }
    DEBUG_PRINT("vgpu_reg_write finished\n");
}

static const MemoryRegionOps vgpu_reg_ops = {
    .read = vgpu_reg_read,
    .write = vgpu_reg_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
        .unaligned = true,
    },
};

static void pci_vgpu_realize(PCIDevice *dev, Error **errp)
{
    PCIVGPUState *d = PCI_VGPU(dev);
    VGACommonState *s = &d->vga;

    /* setup VGA */
    vga_common_init(s, OBJECT(dev));
    vga_init(s, OBJECT(dev), pci_address_space(dev), pci_address_space_io(dev),
            true);
    s->con = graphic_console_init(DEVICE(dev), 0, s->hw_ops, s);

    /* memory region */
    // FIXME: cannot reserve BAR0 in guest device driver

    pci_register_bar(&d->dev, 1, PCI_BASE_ADDRESS_MEM_PREFETCH, &s->vram);

    memory_region_init_io(&d->reg_bar, OBJECT(dev), &vgpu_reg_ops, d, "vgpu.reg",
            VGPU_REG_SIZE);
    pci_register_bar(&d->dev, 2, PCI_BASE_ADDRESS_SPACE_MEMORY, &d->reg_bar);

    d->dstore_size = AVA_GUEST_SHM_SIZE;
    d->dstore_ptr = (void *)((uintptr_t)kvm_get_shm_base(kvm_state) +
                  AVA_GUEST_SHM_SIZE * (kvm_get_vm_id(kvm_state) - 1));
    memory_region_init_ram_ptr(&d->dstore_ram, NULL, "vgpu.dstore", d->dstore_size,
            d->dstore_ptr);
    pci_register_bar(&d->dev, 4, PCI_BASE_ADDRESS_MEM_PREFETCH, &d->dstore_ram);

    /* expose zero-copy region to guest VM. */
    if (ava_get_zcopy_fd(kvm_state) > 0) {
        d->zcopy_size = VGPU_ZERO_COPY_SIZE;
        d->zcopy_ptr = ava_get_zcopy_base(kvm_state);
        memory_region_init_ram_ptr(&d->zcopy_ram, NULL, "vgpu.zcopy", d->zcopy_size,
                d->zcopy_ptr);
        pci_register_bar(&d->dev, 5, PCI_BASE_ADDRESS_MEM_PREFETCH, &d->zcopy_ram);
    }
}

static void pci_vgpu_reset(DeviceState *dev) {
    fprintf(stderr, "pci_vgpu_reset under development\n");

    PCIVGPUState *d = PCI_VGPU(PCI_DEVICE(dev));
    vga_common_reset(&d->vga);
}

static Property pci_vgpu_properties[] = {
    DEFINE_PROP_UINT32("vram_mb", struct PCIVGPUState,
            vga.vram_size_mb, VGPU_VRAM_SIZE),
    DEFINE_PROP_END_OF_LIST(),
};

static void pci_vgpu_class_init(ObjectClass *kclass, void *data) {
    DeviceClass *dc = DEVICE_CLASS(kclass);
    PCIDeviceClass *k = PCI_DEVICE_CLASS(kclass);

    k->realize = pci_vgpu_realize;
    k->romfile = "vgabios-vgpu.bin";

    /* identify the device */
    k->vendor_id = PCI_VENDOR_ID_VGPU;
    k->device_id = PCI_DEVICE_ID_VGPU;
    k->class_id = PCI_CLASS_DISPLAY_VGA;
    set_bit(DEVICE_CATEGORY_DISPLAY, dc->categories);
    dc->desc = "SCEA General vGPU in test";
    k->revision = 0x00;

    /* qemu user things */
    dc->props = pci_vgpu_properties;
    dc->hotpluggable = false;
    dc->reset = pci_vgpu_reset;
}

static const TypeInfo vgpu_info = {
    .name          = TYPE_PCI_VGPU,
    .parent        = TYPE_PCI_DEVICE,
    .instance_size = sizeof(struct PCIVGPUState),
    .class_init    = pci_vgpu_class_init,
    .interfaces    = (InterfaceInfo[]) {
        { INTERFACE_CONVENTIONAL_PCI_DEVICE },
        { },
    },
};

static void vgpu_register_types(void) {
    type_register_static(&vgpu_info);
}

type_init(vgpu_register_types);
