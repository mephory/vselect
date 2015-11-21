all: program

program:
	gcc -o vselect $(shell pkg-config --cflags --libs cairo x11) vselect.c

clean:
	rm vselect
