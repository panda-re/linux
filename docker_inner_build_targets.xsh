#!/usr/bin/env xonsh
# run inside docker guest; based on setup.sh and vsock_vpn docker_inner_build_targets.sh
import os
import re

$XONSH_TRACE_SUBPROC = True
$UPDATE_OS_ENVIRON = True

if "IGLOO_TARGETLIST" not in os.environ:
    TARGETLIST="armel mipseb mipsel".split()
else:
    TARGETLIST=os.environ["IGLOO_TARGETLIST"].split()

if "DOCKER_USER" not in os.environ:
    $DOCKER_USER=$USER
$CONSOLE="/console"
$PANDA="/panda"
$BUILD_ROOT="/linux" #/linux is the volume mount point for this directory
$OUT="/linux/binaries"

echo f"Building for {TARGETLIST} arches"

def get_cc(arch):
    #Clear CFLAGS if we set them
    if "CFLAGS" in os.environ:
        del $CFLAGS
    if "KCFLAGS" in os.environ:
        del $KCFLAGS
    if "arm" in arch:
        abi="eabi"
        if "eb" in arch:
            $CFLAGS="-mbig-endian"
            $KCFLAGS="-mbig-endian"
        arch = "arm"
    else:
        abi=""

    #Kernel CROSS_COMPILE will append tool names as needed, so we won't here
    return f"/opt/cross/{arch}-linux-musl{abi}/bin/{arch}-linux-musl{abi}-"

rm -rf $OUT
mkdir -p $OUT


$NPROC=$(nproc).strip()

#Now build kernels
cd $BUILD_ROOT

for arch in TARGETLIST:
    make mrproper
    TARGETS=["vmlinux"]
    if "arm" in arch:
        TARGETS.append("zImage")

    m=re.match(r"(.*)e[lb]",arch)
    short_arch=m.group(1)
    if short_arch == "mips64":
        short_arch = "mips"


    $CROSS_CC=get_cc(arch)
    mkdir -p f"build/{arch}"
    cp f"config.{arch}" f"build/{arch}/.config"
    make ARCH=@(short_arch) CROSS_COMPILE=$CROSS_CC O=build/@(arch) olddefconfig

    echo "Building kernel"
    make ARCH=@(short_arch) CROSS_COMPILE=$CROSS_CC O=build/@(arch) @(TARGETS) -j$NPROC

    echo 'Updating PANDA info'
    @($PANDA)/panda/plugins/osi_linux/utils/kernelinfo_gdb/run.sh ./build/@(arch)/vmlinux ./panda_profile.@(arch)

    if os.path.exists(f'build/{arch}/arch/{short_arch}/boot/zImage'):
      cp  f'build/{arch}/arch/{short_arch}/boot/zImage' @($OUT)/zImage.@(arch)

    cp build/@(arch)/vmlinux @($OUT)/vmlinux.@(arch)
    echo f"[{arch}]" >> @($OUT)/firmadyne_profiles.conf
    cat ./panda_profile.@(arch) >> @($OUT)/firmadyne_profiles.conf

    echo 'Building volatility profile'
    /dwarf2json/dwarf2json linux --elf build/@(arch)/vmlinux | xz - > @($OUT)/vmlinux.@(arch).json.xz

git config --global --add safe.directory /linux
echo f"Built by {$DOCKER_USER} on {$(date).strip()} at version {$(git describe HEAD).strip()}" > @($OUT)/README.txt
