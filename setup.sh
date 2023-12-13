#!/bin/sh
set -eux

# valid: arm, mipsel, mipseb, mips64eb.
# Need to map to short_arch for make target
# Need  config.${arch} file
# and need /cross/${ARCH}... cross compiler
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
elif [ "$ARCH" = "mips64eb" ] || [ "$ARCH" = "mips64el" ]; then
  if [ "$ARCH" = "mips64el" ]; then
    echo "mips64el unsupported"
    exit 1
  fi
  SHORT_ARCH=mips
  CROSS_CC=/opt/cross/mips64-linux-musl/bin/mips64-linux-musl-
else
  CROSS_CC=/cross/${ARCH}-linux-musl${ABI}/bin/${ARCH}-linux-musl${ABI}-
fi

if [ ! -e build/${ARCH}/.config ] || [ "$(diff build/${ARCH}/.config config.${ARCH} | wc -l)" -ne 0 ];  then
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

#echo 'Building volatility profile'
#dwarf2json wants a newer go than ships with Ubuntu 20.04, so I did this:
#mkdir -p ~/go-1.14.2
#wget -c https://dl.google.com/go/go1.14.2.linux-amd64.tar.gz -O - | tar -xz -C ~/go-1.14.2
#~/go-1.14.2/go/bin/go
#DWARF2JSON=~/git/dwarf2json/dwarf2json
#${DWARF2JSON} linux --elf build/${ARCH}/vmlinux | xz - > ./vmlinux.${ARCH}.json.xz
