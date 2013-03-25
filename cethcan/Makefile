love: cethcan
.PHONY: love

PKGS="libevent jansson"

cethcan: main.o can.o
	gcc -g -o $@ `pkg-config --libs $(PKGS)` $^

clean:
	rm -f *.o *.y.c *.y.h *.l.c cethcan

%.o: %.c *.h
	gcc -c -g -O0 -Wall -Wextra -Wshadow -pedantic -Wno-unused-parameter -Wno-format -std=gnu99 `pkg-config --cflags $(PKGS)` -o $@ $<

