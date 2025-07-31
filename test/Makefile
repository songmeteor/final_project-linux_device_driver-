obj-m := rotary_test.o
KDIR := $(HOME)/project2/linux
PWD  := $(shell pwd)

all:	
	make -C $(KDIR) M=$(PWD) modules
  
clean:
	make -C $(KDIR) M=$(PWD) clean
