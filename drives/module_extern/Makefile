#Makefile 
DEBUG = y
ifeq ($(DEBUG), y)
        DEBFLAGS = -O -g
else
        DEBFLAGS = -O2
endif
CFLAGS +=$(DEBFLAGS)
EXTRA_CFLAGS += $(DEBFLAGS)
ifeq ($(KERNELRELEASE),)

KDIR ?= /lib/modules/$(shell uname -r)/build

PWD :=$(shell pwd)
modules:
	$(MAKE) -C $(KDIR) M=$(PWD) modules
modules_install:
	$(MAKE) -C $(KDIR) M=$(PWD) modules_install
clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
.PHONY: modules modules_install clean
else
 		obj-m := scull.o
    obj-m += use_scull.o
endif
############# Makefile end here
