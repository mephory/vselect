all: program

program:
	gcc -o vselect -I/usr/include/cairo -I/usr/include/pixman-1 -I/usr/include/freetype2 -I/usr/include/libpng16 -I/usr/include/harfbuzz -I/usr/include/freetype2 -I/usr/include/harfbuzz -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include -I/usr/include/libdrm -I/usr/include/libpng16 -lcairo -lX11 vselect.c

clean:
	rm vselect
