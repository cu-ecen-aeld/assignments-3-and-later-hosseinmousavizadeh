CC:= gcc

ifeq ($(CROSS_COMPILE), aarch64-none-linux-gnu-)
	CC: aarch64-none-linux-gnu-gcc
endif

aesdsocket: aesdsocket.c
	$(CC) aesdsocket.c -o aesdsocket

all: aesdsocket

default: aesdsocket

clean: 
	rm -f aesdsocket
	rm -f *.o