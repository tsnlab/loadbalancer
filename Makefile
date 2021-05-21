APP = loadbalancer

.PHONY: dependencies clean

#packetvisor
INCLUDES += -Ilib/packetvisor/libpv/include
LDFLAGS_SHARED += -Llib/packetvisor/libpv -lpv

#sglib
INCLUDES += -Ilib/sglib

CFLAGS += -Wall -Wl,-rpath=.

ifeq ($(DEBUG), 1)
	CFLAGS += -O0 -g
else
	CFLAGS += -O3
endif


$(APP): $(wildcard src/*.c) | dependencies
	$(CC) $(CFLAGS) $? -o $@ $(INCLUDES) $(LDFLAGS) $(LDFLAGS_SHARED)


dependencies: .gitmodules
	git submodule update --init
	make -C lib

clean:
	make -C lib clean
	rm $(APP)
