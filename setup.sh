#!/bin/sh
set -eux

# valid: arm, mipsel, mipseb
ARCH=$1

SHORT_ARCH=mips
ABI=
TARGETS=vmlinux


# Firmadyne cross compilers from https://zenodo.org/record/4922202
if [ "$ARCH" = "armel" ] || [ "$ARCH" = "armeb" ]; then
  TARGETS="vmlinux zImage" # only for arm
  ABI=eabi # only for arm
  SHORT_ARCH=arm

  if [ "$ARCH" = "armeb" ]; then
    export CFLAGS="-mbig-endian"
    export KCFLAGS="-mbig-endian"
  fi

  # Even for armeb, we use same compiler. Big endian just needs an extra cflag
  CROSS_CC=/cross/arm-linux-musleabi/bin/arm-linux-musleabi-
else
  CROSS_CC=/cross/${ARCH}-linux-musl${ABI}/bin/${ARCH}-linux-musl${ABI}-
fi

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
