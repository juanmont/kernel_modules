obj-m = fifomod.o #fifomod.c no ha de existir

fifomod-objs = fifoproc.o cbuffer.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
