APP = loadbalancer

.PHONY: dependencies

#packetvisor
INCLUDES += -Ilib/packetvisor/libpv/include
LDFLAGS_SHARED += -Llib/packetvisor/libpv -lpv

#sglib
INCLUDES += -Ilib/sglib

CFLAGS += -Wall

ifeq ($(DEBUG), 1)
	CFLAGS += -O0 -g
else
	CFLAGS += -O3
endif


$(APP): $(wildcard src/*.c) | dependencies
	$(CC) $(CFLAGS) $< -o $@ $(INCLUDES) $(LDFLAGS) $(LDFLAGS_SHARED)


dependencies: .gitmodules
	git submodule update --init
	make -C lib
