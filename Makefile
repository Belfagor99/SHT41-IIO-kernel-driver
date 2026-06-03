obj-m := sht41.o
KDIR  := /sources/linux
PWD   := $(shell pwd)
__BUILD_ARCH := amd64


all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules


clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -rf .clangd .cache *.gz compile_commands.json compile_commands.json.in bzImage .config


# Specific staging targets
copy-to-fs:
	mkdir -p /staging/initramfs/fs/modules
	cp sht41.ko /staging/initramfs/fs/modules/
	mkdir -p /staging/initramfs/fs/usr/bin
	cp sht41_read /staging/initramfs/fs/usr/bin/
	chmod +x /staging/initramfs/fs/usr/bin/sht41_read


test: all copy-to-fs


# https://github.com/Mange/rtl8192eu-linux-driver/issues/205#issuecomment-736366347
prepare: 
	cp ../aux/module.lds.S-$(__BUILD_ARCH) /sources/linux/scripts/module.lds


compile_commands.json:
	make -C /sources/linux M=$(PWD) $@


setup-cc: 
	rm -f compile_commands.json
	make compile_commands.json
	cp compile_commands.json compile_commands.json.in
	cat compile_commands.json.in | sed $(CCSED) > compile_commands.json

	
rebuild: copy-to-fs
	make -C .. rebuild






# Generic Build Target Variables (For compiling natively on any standard Linux Host machine)
KVERSION      ?= $(shell uname -r)
KDIR_GENERIC  ?= /lib/modules/$(KVERSION)/build


#GENERIC TARGET: Builds the module out-of-tree for the current host system natively
generic:
	@echo "Building sht41 module generically for host kernel version: $(KVERSION)"
	$(MAKE) -C $(KDIR_GENERIC) M=$(PWD) modules
