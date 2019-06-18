FROM ubuntu:19.04
MAINTAINER adwe <adwe@live.se>
RUN apt-get update && apt-get install -y \
        wget \
        gpg \
        make \
        gcc

RUN wget https://www.musl-libc.org/releases/musl-1.1.22.tar.gz https://www.musl-libc.org/releases/musl-1.1.22.tar.gz.asc https://www.musl-libc.org/musl.pub && gpg --import musl.pub && gpg --verify musl-1.1.22.tar.gz.asc && tar xf musl-1.1.22.tar.gz

ENV MUSL_SRC_DIR /musl-1.1.22
ENV MUSL_BUILD_DIR /build/musl

WORKDIR $MUSL_BUILD_DIR
# install musl at /usr/local/musl
RUN $MUSL_SRC_DIR/configure && make && make install

# libdwarf
RUN apt-get update && apt-get install -y \
        git \
        autoconf \
        libtool \
        autopoint

WORKDIR /
ENV LIBELF_SRC_DIR /libelf
ENV LIBELF_BUILD_DIR /build/libelf
RUN git clone https://github.com/vtorri/libelf.git $LIBELF_SRC_DIR
WORKDIR $LIBELF_BUILD_DIR
RUN autoreconf --install $LIBELF_SRC_DIR && CC=/usr/local/musl/bin/musl-gcc $LIBELF_SRC_DIR/configure --prefix=/usr/local/musl && make && make install

WORKDIR /
ENV ZLIB_SRC_DIR /zlib-1.2.11
ENV ZLIB_BUILD_DIR /build/zlib
RUN wget https://zlib.net/zlib-1.2.11.tar.gz https://zlib.net/zlib-1.2.11.tar.gz.asc https://madler.net/madler/pgp.html && gpg --import pgp.html && gpg --verify zlib-1.2.11.tar.gz.asc && tar xf zlib-1.2.11.tar.gz
WORKDIR $ZLIB_BUILD_DIR
RUN CC=/usr/local/musl/bin/musl-gcc /zlib-1.2.11/configure --prefix /usr/local/musl && make && make install

ENV LIBDWARF_SRC_DIR /libdwarf
ENV LIBDWARF_BUILD_DIR /build/libdwarf
RUN git clone --single-branch --branch 20190529 git://git.code.sf.net/p/libdwarf/code $LIBDWARF_SRC_DIR
WORKDIR $LIBDWARF_BUILD_DIR
RUN autoreconf --install $LIBDWARF_SRC_DIR && CC=/usr/local/musl/bin/musl-gcc $LIBDWARF_SRC_DIR/configure --prefix=/usr/local/musl && make && make install

COPY addr2line.c /addr2line/
COPY Makefile /addr2line/
WORKDIR /addr2line
RUN make CC=/usr/local/musl/bin/musl-gcc LDIR=/usr/local/musl/lib IDIR=/usr/local/musl/include LDFLAGS=-static
