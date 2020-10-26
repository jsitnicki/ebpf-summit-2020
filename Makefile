# SPDX-License-Identifier: (GPL-2.0-only OR BSD-2-Clause)
# Copyright (c) 2020 Cloudflare

KERNEL := linux-5.9.1
KERNEL_INC := $(KERNEL)/usr/include
LIBBPF_INC := $(KERNEL)/tools/lib
LIBBPF_LIB := $(KERNL)/tools/lib/bpf/libbpf.a

CC := clang
CFLAGS := -g -O2 -Wall -Wextra
CPPFLAGS := -I$(KERNEL_INC) -I$(LIBBPF_INC)

PROGS := sockmap-update sk-lookup-attach echo_dispatch.bpf.o bpftool

.PHONY: all
all: $(PROGS)

sockmap-update: sockmap_update.c $(KERNEL_INC)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $<

sk-lookup-attach: sk_lookup_attach.c $(KERNEL_INC)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $<

echo_dispatch.bpf.o: echo_dispatch.bpf.c $(KERNEL_INC) $(LIBBPF_INC) $(LIBBPF_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) -target bpf -c -o $@ $<

# Download kernel sources
$(KERNEL).tar.xz:
	curl -O https://cdn.kernel.org/pub/linux/kernel/v5.x/$(KERNEL).tar.xz

# Unpack kernel sources
$(KERNEL): $(KERNEL).tar.xz
	tar axf $<

# Install kernel headers
$(KERNEL_INC): $(KERNEL)
	make -C $< headers_install INSTALL_HDR_PATH=$@

# Build libbpf to generate helper definitions header
$(LIBBPF_LIB): $(KERNEL)
	make -C $</tools/lib/bpf

# Build bpftool
bpftool: $(KERNEL)
	make -C $</tools/bpf/bpftool
	cp $</tools/bpf/bpftool/bpftool $@

.PHONY: clean
clean:
	rm -f $(PROGS)

.PHONY: dist-clean
dist-clean: clean
	rm -rf $(KERNEL) $(KERNEL).tar.xz
