obj-m += sr04.o
KDIR = /lib/modules/$(shell uname -r)/build
all:
	make -C $(KDIR) M=$(shell pwd) modules
	sudo mokutil --import MOK.der
	kmodsign sha512 MOK.priv MOK.der sr04.ko sr04.ko
clean:
	make -C $(KDIR) M=$(shell pwd) clean
