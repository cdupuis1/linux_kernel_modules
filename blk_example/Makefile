obj-m += blk_example.o

all:
	make -C/lib/modules/$(shell uname -r)/build M=$(PWD) EXTRA_CFLAGS="-g" modules
	./sign_module.sh

clean:
	make -C/lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm -f *.priv *.x509
