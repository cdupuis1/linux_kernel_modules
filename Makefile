obj-m += nvme_test.o
obj-m += blk_example.o

all:
	make -C/lib/modules/$(shell uname -r)/build M=$(PWD) EXTRA_CFLAGS="-g" modules
clean:
	make -C/lib/modules/$(shell uname -r)/build M=$(PWD) clean
