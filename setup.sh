#!/bin/sh
set -eux

# valid: arm, mipsel, mipseb
ARCH=$1

SHORT_ARCH=mips
ABI=
TARGETS=vmlinux


if [ "$ARCH" = "arm" ]; then
  TARGETS="vmlinux zImage" # only for arm
  ABI=eabi # only for arm
  SHORT_ARCH=$ARCH
fi

# Firmadyne cross compilers from https://zenodo.org/record/4922202
# works for kernel 4.10
CROSS_CC=/cross/${ARCH}-linux-musl${ABI}/bin/${ARCH}-linux-musl${ABI}-

if [ ! -e build/${ARCH}/.config ] || [ "$(diff build/${ARCH}/.config config.${ARCH} | wc -l)" -eq 0 ];  then
  echo "Configuring kernel"
  mkdir -p build/${ARCH}
  cp config.${ARCH} build/${ARCH}/.config
  make ARCH=$SHORT_ARCH CROSS_COMPILE=${CROSS_CC} O=build/${ARCH} olddefconfig
else
  echo "No need to reconfigure kernel"
fi

echo "Building kernel"
make ARCH=${SHORT_ARCH} CROSS_COMPILE=${CROSS_CC} O=build/${ARCH} $TARGETS -j$(nproc)

#PANDA=~/git/panda
#echo 'Updating PANDA info'
#${PANDA}/panda/plugins/osi_linux/utils/kernelinfo_gdb/run.sh ./build/${ARCH}/vmlinux ./panda_profile.${ARCH}
#
#if [ -e build/${ARCH}/arch/${SHORT_ARCH}/boot/zImage ]; then
#  cp build/${ARCH}/arch/${SHORT_ARCH}/boot/zImage  ${OUTDIR}/zImage4.${ARCH}
#fi

#echo "[${ARCH}]" > ${OUTDIR}/${ARCH}_profile4.conf 
#cat panda_profile.${ARCH} >> ${OUTDIR}/${ARCH}_profile4.conf 
