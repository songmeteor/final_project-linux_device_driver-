obj-m += vs10xx.o
vs10xx-objs := vs10xx_main.o vs10xx_device.o vs10xx_iocomm.o vs10xx_queue.o

KDIR := $(HOME)/project2/linux
PWD := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

install:
	$(MAKE) -C $(KDIR) M=$(PWD) modules_install


