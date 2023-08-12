obj-m += misc_example.o

KERNEL_DIR= /lib/modules/$(shell uname -r)/build

all:
	make -C $(KERNEL_DIR) M=$(shell pwd) modules
	gcc test.c -o test

clean:
	make -C $(KERNEL_DIR) M=$(shell pwd) clean
	rm -f test

