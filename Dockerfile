FROM golang:latest as go
RUN git clone --depth 1 https://github.com/volatilityfoundation/dwarf2json.git \
    && cd dwarf2json \
    && go build 

FROM ghcr.io/panda-re/embedded-toolchains:latest
RUN apt-get update && apt-get -y install gdb xonsh 

# Get panda for kernelinfo_gdb. Definitely a bit overkill to pull the whole repo
RUN git clone --depth 1 https://github.com/panda-re/panda.git
COPY --from=go /go/dwarf2json/dwarf2json /bin/dwarf2json