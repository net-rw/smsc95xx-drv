# SPDX-License-Identifier: GPL-2.0
obj-m	+= smsc95xx.o

KDIR  := /lib/modules/$(shell uname -r)/build
MDIR  := /lib/modules/$(shell uname -r)
PWD   := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

install: smsc95xx.ko
	rm -f ${MDIR}/kernel/drivers/net/usb/smsc95xx.ko
	install -m644 -b -D smsc95xx.ko ${MDIR}/kernel/drivers/net/usb/smsc95xx.ko
	depmod -aq

.PHONY : all clean install
