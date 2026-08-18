obj-m += r8139.o

# KDIR ?= /lib/modules/$(shell uname -r)/build
KDIR ?= /home/liang/Desktop/code/linux-kgdb

PWD := $(CURDIR)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

install:
	$(MAKE) -C $(KDIR) M=$(PWD) modules_install
	depmod -a

.PHONY: all clean install
