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

SRCS = $(wildcard src/*.c)
OBJS = $(patsubst src/%.c, obj/%.o, $(SRCS))
DEPS = $(OBJS:.o=.d)


$(APP): $(OBJS) | dependencies
	$(CC) $(CFLAGS) $? -o $@ $(LDFLAGS) $(LDFLAGS_SHARED)

obj:
	mkdir -p obj

obj/%.d : src/%.c | obj
	$(CC) $(CFLAGS) -M -MT $(@:.d=.o) $< -o $@ $(INCLUDES)

obj/%.o: src/%.c | obj
	$(CC) $(CFLAGS) -c -o $@ $< $(INCLUDES) $(LDFLAGS)


dependencies: .gitmodules
	git submodule update --init
	make -C lib

clean:
	make -C lib clean
	rm $(APP)

ifneq (clean,$(filter clean,$(MAKECMDGOALS)))
-include $(DEPS)
endif
