love: cethcan
.PHONY: love

PKGS="libevent jansson"
L_CFLAGS=-g -O0 -pthread -Wall -Wextra -Wshadow -pedantic -Wno-unused-parameter -Wno-format -std=gnu11 `pkg-config --cflags $(PKGS)` $(CFLAGS)
L_LDFLAGS=-g -pthread `pkg-config --libs $(PKGS)` -lcrypto -lcosc $(LDFLAGS)

cethcan: main.o can.o ether.o light.o beanctr.o \
		http.o socketcan.o jsonrpc.o rpc.o espnet.o ttydmx.o \
		osc.o
	gcc $(L_LDFLAGS) -o $@ $^

clean:
	rm -f *.o *.y.c *.y.h *.l.c cethcan

jsonrpc.o: jsonrpc/jsonrpc.c jsonrpc/jsonrpc.h
	gcc $(L_CFLAGS) -c -o $@ $<

%.o: %.c *.h
	gcc $(L_CFLAGS) -c -o $@ $<

