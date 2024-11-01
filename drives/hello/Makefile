KERNDIR = /lib/modules/$(shell uname -r)/build
PWD = $(shell pwd)

obj-m:=virhello.o

all:
	make -C $(KERNDIR) M=$(PWD) modules
	
clean:
	make -C $(KERNDIR) M=$(PWD) clean
