cc := gcc
ifeq ($(CROSS_COMPILE), aarch64-none-linux-gnu-)
	cc := aarch64-none-linux-gnu-gcc
endif

writer: writer.c
	$(cc) writer.c -o writer
clean:
	rm -f writer
	rm -f *.o
