.PHONY: all clean

all: packetvisor/libpv/libpv.so

packetvisor/libpv/libpv.so:
	DEBUG=0 make -C packetvisor

clean:
	make -C packetvisor clean
