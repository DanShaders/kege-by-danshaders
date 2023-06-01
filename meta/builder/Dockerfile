FROM debian:testing-slim

# Install "build essentials"
RUN apt-get update &&     \
    apt-get upgrade -y && \
    apt-get install -y    \
        clang-15          \
        cmake             \
        curl              \
        g++-12            \
        gpg               \
        make              \
        ninja-build

# Add third-party repositories
RUN curl -s https://deb.nodesource.com/gpgkey/nodesource.gpg.key | gpg --dearmor | tee /usr/share/keyrings/nodesource.gpg >/dev/null &&\
    echo 'deb [signed-by=/usr/share/keyrings/nodesource.gpg] https://deb.nodesource.com/node_16.x bookworm main' > /etc/apt/sources.list.d/nodesource.list && \
    printf 'Package: *\nPin: origin deb.nodesource.com\nPin-Priority: 1000\n' > /etc/apt/preferences && \
    apt-get update

# Install nodejs-16 & libpq
RUN apt-get install -y nodejs libpq-dev

RUN apt-get install -y   \
    git                  \
    libcurl4-openssl-dev \
    libfcgi-dev          \
    libssl-dev           \
    pkg-config           \
    systemtap-sdt-dev    \
    unzip

# protobuf 3.20.3
# FIXME: Update protoc to the most recent version when they fix js codegen
RUN mkdir -p /third-party && cd /third-party && \
    curl -SLO https://github.com/protocolbuffers/protobuf/releases/download/v3.20.3/protobuf-js-3.20.3.zip && \
    unzip protobuf-js-3.20.3.zip && \
    cd protobuf-3.20.3 && \
    CC=gcc-12 CXX=g++-12 ./configure --prefix=/usr && make install -j`nproc` && \
    cd / && rm -r /third-party/

# fmt 9.1.0
RUN mkdir -p /third-party/fmt && \
    git clone https://github.com/fmtlib/fmt.git /third-party/fmt && \
    cd /third-party/fmt && \
    git checkout a33701196adfad74917046096bf5a2aa0ab0bb50 && \
    cmake \
        -DCMAKE_C_COMPILER=gcc-12 \
        -DCMAKE_CXX_COMPILER=g++-12 \
        -DFMT_TEST=OFF -DFMT_DOC=OFF \
        -G Ninja -B Build . && \
    ninja -C Build install && \
    cd / && rm -r /third-party/

# Remaining configuration
ARG uid

RUN git config --global init.defaultBranch master && \
    ln -s /usr/bin/g++-12 /usr/bin/g++ && \
    useradd -u $uid -m user && \
    install -d -m 0755 -o user -g user /kege && \
    apt-get install -y sudo && \
    echo 'user ALL=(ALL) NOPASSWD:ALL' >> /etc/sudoers

USER user
