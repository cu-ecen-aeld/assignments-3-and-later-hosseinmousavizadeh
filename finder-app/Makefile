cc := gcc
ifeq ($(CROSS_COMPILE), aarch64-none-linux-gnu-)
	cc := aarch64-none-linux-gnu-gcc
endif

$(info CC is [$(CC)])
$(info CROSS_COMPILE is [$(CROSS_COMPILE)])

writer: writer.c
	$(CC) writer.c -o writer
clean:
	rm -f writer
	rm -f *.o
