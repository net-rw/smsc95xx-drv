# SPDX-License-Identifier: GPL-2.0

ifneq ($(KERNELRELEASE),)

obj-$(CONFIG_SMSC95XX_DRV) += smsc95xx.o

smsc95xx-objs := \
	smsc95xx-main.o

else

KDIR  := /lib/modules/$(shell uname -r)/build
MDIR  := /lib/modules/$(shell uname -r)
PWD   := $(shell pwd)

export CONFIG_SMSC95XX_DRV := m

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

install: smsc95xx.ko
	rm -f ${MDIR}/kernel/drivers/net/usb/smsc95xx.ko
	install -m644 -b -D smsc95xx.ko ${MDIR}/kernel/drivers/net/usb/smsc95xx.ko
	depmod -aq

endif

.PHONY : all clean install
