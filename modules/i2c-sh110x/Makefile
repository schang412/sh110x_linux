
ifneq ($(KERNELRELEASE),)

	# object files to build
	obj-m += i2c-sh110x.o

else

	ifneq ($(KERNEL_SRC),)
		# accept KERNEL_SRC as an alternative KDIR variable
		KDIR ?= $(KERNEL_SRC)
	endif

	KDIR ?= /lib/modules/$(shell uname -r)/build

all:
	$(MAKE) -C $(KDIR) M=$(shell pwd) modules

help modules modules_install clean:
	$(MAKE) -C $(KDIR) M=$(shell pwd) $@

install: modules_install

endif
