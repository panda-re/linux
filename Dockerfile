FROM ubuntu:22.04
RUN apt-get update
RUN apt-get -y install build-essential gdb golang-go xonsh git wget libncurses-dev bc
RUN git clone --depth 1 https://github.com/volatilityfoundation/dwarf2json.git \
    && cd dwarf2json \
    && go build 
RUN mkdir -p /opt/cross
RUN wget https://musl.cc/i686-linux-musl-cross.tgz -O - | tar -xz -C /opt/cross && ln -s /opt/cross/i686-linux-musl-cross /opt/cross/i686-linux-musl
RUN wget https://musl.cc/x86_64-linux-musl-cross.tgz -O - | tar -xz -C /opt/cross && ln -s /opt/cross/x86_64-linux-musl-cross /opt/cross/x86_64-linux-musl
#Latest mips and mipsel toolchains break on building old kernels so we use these with gcc 5.3.0
RUN wget http://panda.re/secret/mipseb-linux-musl_gcc-5.3.0.tar.gz -O - | tar -xz -C /opt/cross
RUN wget http://panda.re/secret/mipsel-linux-musl_gcc-5.3.0.tar.gz -O - | tar -xz -C /opt/cross
#mips64 toolchain built using https://github.com/richfelker/musl-cross-make
#BINUTILS_VER = 2.25.1
#GCC_VER = 6.5.0
#MUSL_VER = git-v1.1.24
#GMP_VER = 6.1.0
#MPC_VER = 1.0.3
#MPFR_VER = 3.1.4
#GCC_CONFIG += --enable-languages=c
#It's a bit nutty to symlink all of these, but easier to keep track of what's needed for the future
RUN wget http://panda.re/secret/mips64-linux-musl-cross_gcc-6.5.0.tar.gz -O - | tar -xz -C /opt/cross &&  ln -s /opt/cross/mips64-linux-musl-cross /opt/cross/mips64eb-linux-musl && \ 
    ln -s /opt/cross/mips64eb-linux-musl/bin/mips64-linux-musl-gcc /opt/cross/mips64eb-linux-musl/bin/mips64eb-linux-musl-gcc && \
    ln -s /opt/cross/mips64eb-linux-musl/bin/mips64-linux-musl-ld /opt/cross/mips64eb-linux-musl/bin/mips64eb-linux-musl-ld && \
    ln -s /opt/cross/mips64eb-linux-musl/bin/mips64-linux-musl-objdump /opt/cross/mips64eb-linux-musl/bin/mips64eb-linux-musl-objdump && \
    ln -s /opt/cross/mips64eb-linux-musl/bin/mips64-linux-musl-objcopy /opt/cross/mips64eb-linux-musl/bin/mips64eb-linux-musl-objcopy && \
    ln -s /opt/cross/mips64eb-linux-musl/bin/mips64-linux-musl-ar /opt/cross/mips64eb-linux-musl/bin/mips64eb-linux-musl-ar && \
    ln -s /opt/cross/mips64eb-linux-musl/bin/mips64-linux-musl-nm /opt/cross/mips64eb-linux-musl/bin/mips64eb-linux-musl-nm
RUN wget https://musl.cc/mips64el-linux-musl-cross.tgz -O -  | tar -xz -C /opt/cross &&  ln -s /opt/cross/mips64el-linux-musl-cross /opt/cross/mips64el-linux-musl 
RUN wget https://musl.cc/arm-linux-musleabi-cross.tgz -O - | tar -xz -C /opt/cross &&  ln -s /opt/cross/arm-linux-musleabi-cross /opt/cross/arm-linux-musleabi
RUN wget https://musl.cc/aarch64-linux-musl-cross.tgz -O - | tar -xz -C /opt/cross && ln -s /opt/cross/aarch64-linux-musl-cross /opt/cross/aarch64-linux-musl 

# We ship console binaries in the same file, so pull those down as well
RUN git clone --depth 1 https://github.com/AndrewFasano/console.git

# Get panda for kernelinfo_gdb. Definitely a bit overkill to pull the whole repo
# However, we want a newer version of go for dwarf2json.
RUN git clone --depth 1 https://github.com/panda-re/panda.git
