#ifndef __VGA_VGPU_H__
#define __VGA_VGPU_H__

#include "hw/hw.h"
#include "hw/pci/pci.h"
#include "ui/console.h"
#include "vga_int.h"
#include "qemu/thread.h"

#include <stddef.h>
#include <stdint.h>

#include "vgpu-common/devconf.h"
#include "vgpu-common/ioctl.h"
#include "vgpu-common/register.h"

typedef struct PCIVGPUState {
    PCIDevice dev;
    VGACommonState vga;

    MemoryRegion reg_bar;

    MemoryRegion io_bar;

    MemoryRegion dstore_ram;
    void *dstore_ptr;
    unsigned int dstore_size;

    MemoryRegion zcopy_ram;
    void *zcopy_ptr;
    unsigned long zcopy_size;

    QemuThread kvm_wait_queue_thead;

    int device_id;
    int bustype;

    unsigned int vm_id;
    void *shm_registry;
} PCIVGPUState;

#define TYPE_PCI_VGPU VGPU_DEV_NAME
#define PCI_VGPU(obj) \
    OBJECT_CHECK(PCIVGPUState, (obj), TYPE_PCI_VGPU)

/*******************************************/

/* BAR4 DSTORE */

#define get_ptr_from_dstore(_state, _ptr, _type) \
    ((_ptr != NULL) ? \
     (_type)((uintptr_t)_state->dstore_ptr + (uintptr_t)_ptr) : \
     NULL);

/* BAR5 IO ports */

#endif
