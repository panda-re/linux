#!/bin/sh

set -eux

rm -rf kernels
mkdir kernels

./setup.sh arm
./setup.sh mipsel
./setup.sh mipseb
./setup.sh mips64eb

cp build/arm/arch/arm/boot/zImage kernels/zImage.armel
cp build/arm/vmlinux kernels/vmlinux.armel

cp build/mipseb/vmlinux kernels/vmlinux.mipseb
cp build/mipsel/vmlinux kernels/vmlinux.mipsel

cp build/mips64eb/vmlinux kernels/vmlinux.mips64eb

PROF=kernels/firmadyne_profiles.conf
echo "[armel]" > $PROF
cat panda_profile.arm >> $PROF
echo "[mipseb]" >> $PROF
cat panda_profile.mipseb >> $PROF
echo "[mipsel]" >> $PROF
cat panda_profile.mipsel >> $PROF
echo "[mips64eb]" >> $PROF
cat panda_profile.mips64eb >> $PROF

mv ./vmlinux.arm.json.xz ./vmlinux.armel.json.xz
cp ./vmlinux.*.json.xz kernels/

cp -a console.bins/* kernels/

echo "Built by $(whoami) on $(date) at version $(git describe HEAD)" > kernels/README.txt
