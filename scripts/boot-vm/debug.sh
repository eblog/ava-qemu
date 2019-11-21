#!/usr/bin/sudo /bin/bash

# Set up environment
. $(dirname $0)/scripts/environment

# Set up options
. $(dirname $0)/scripts/options
. $(dirname $0)/scripts/settings
. $(dirname $0)/scripts/bindings

# Specific settings
# NULL

# Run KVM
set -x
gdb -q --args \
${QEMU_BIN} ${MEM_OPT} ${IMAGE} ${!CDROM} ${!VIRTFS} ${!VIRTGPU} \
    ${SMP} ${!GRAPHICS} ${!SOUND} ${!SERIAL} ${!AUTOBALLOON} ${NET} ${!QMP} \
    ${!SNAPSHOT} ${!DEBUG} ${!MONITOR} \
    -enable-kvm -machine accel=kvm -cpu host,kvm=on \
    -device scea-vgpu
