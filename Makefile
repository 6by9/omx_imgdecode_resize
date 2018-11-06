CROSS_COMPILE ?=

CC	:= $(CROSS_COMPILE)g++
CPPFLAGS ?= -I/opt/vc/include -I/opt/vc/include/vmcs_host/khronos/ -pipe -W -Wall -Wextra -g -O0 -DOMX_SKIP64BIT
LDFLAGS	?=
LIBS	:= -L/opt/vc/lib -lrt -lbcm_host -lvcos -lvchiq_arm -lpthread -lopenmaxil -lm -lsupc++

%.o : %.c
	$(CC) $(CPPFLAGS) -c -o $@ $<

all: main

main: main.o ilclient.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

clean:
	-rm -f *.o
	-rm -f main

