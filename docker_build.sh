#!/bin/bash
#Commented out target list is what we'd like to have, but haven't tested
#TARGETLIST="armel armeb mipseb mipsel mips64 mips64el aarch64"
#These targets are ones we've tested
TARGETLIST="armel mipseb mipsel mips64eb mips64el"

docker build -t igloo_kernel_builder . || exit
docker run --rm -v `realpath .`:/linux  -w /linux -e IGLOO_TARGETLIST="$TARGETLIST" -e DOCKER_USER="$USER" \
       igloo_kernel_builder xonsh docker_inner_build_targets.xsh
