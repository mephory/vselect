PREFIX=/usr/local

all: program

program:
	gcc -o vselect $(shell pkg-config --cflags --libs cairo x11) vselect.c

install-utils:
	install -m 0755 utils/* $(prefix)/bin

install:
	install -m 0755 vselect $(prefix)/bin

clean:
	rm vselect
