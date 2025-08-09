obj-m += scsi_sample.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

install:
	sudo insmod scsi_sample.ko size_in_mb=100

remove:
	sudo rmmod scsi_sample

.PHONY: all clean install remove

