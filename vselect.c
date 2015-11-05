// Usage: vselect [OPTION]... FILENAME
//
// Options:
//     -r, --rect          select a rectangular area (default)
//     -p, --point         select a point instead of an area
//     -f, --format=FORMAT specify an output format
//
// FILENAME must be an image file or - for stdin
//
// FORMAT is any string that can contain the following vars:
//     %x      x coordinate (left side if rect)
//     %y      y corrdinate (top side if rect)
//     %w      width (only for area)
//     %h      height (only for area)
//     %x2     width - x (only for area)
//     %y2     height - y (only for area)
//
// The default format is the default geometry syntax imagemagick uses:
//     +%x+%y          for points
//     %wx%h+%x+%y     for rects

#include <cairo.h>
#include <cairo-xlib.h>
#include <X11/Xlib.h>
#include <stdio.h>
#include <stdlib.h>

Display *dpy;
int screen;
Window root;

void initialize_xlib() {
    if (!(dpy=XOpenDisplay(NULL))) {
        fprintf(stderr, "ERROR: Could not open display\n");
        exit(1);
    }

    screen = DefaultScreen(dpy);
    root = RootWindow(dpy, screen);
}

Window create_window(char *name, int width, int height) {
    Window win;
    win = XCreateSimpleWindow(dpy, root, 1, 1, width, height, 0,
        BlackPixel(dpy, screen), BlackPixel(dpy, screen));

    XStoreName(dpy, win, name);
    XSelectInput(dpy, win, ExposureMask | ButtonPressMask);

    return win;
}

cairo_surface_t *open_image(char *filename) {
    return cairo_image_surface_create_from_png(filename);
}

void paint_image(cairo_surface_t *target_surf, cairo_surface_t *image) {
    cairo_t *c;

    c = cairo_create(target_surf);
    cairo_set_source_surface(c, image, 0, 0);
    cairo_paint(c);
}

int main(int argc, char **argv) {
    XEvent ev;
    Window win;
    cairo_surface_t *window_surface;
    cairo_surface_t *image;
    int width, height;

    // Open the image and get the dimensions
    image = open_image("./test.png");
    width = cairo_image_surface_get_width(image);
    height = cairo_image_surface_get_height(image);

    // Initialize and map X Window
    initialize_xlib();
    win = create_window("vselect", width, height);
    XMapWindow(dpy, win);

    window_surface = cairo_xlib_surface_create(dpy, win,
        DefaultVisual(dpy, 0), width + 1, height + 1);

    while (1) {
        XNextEvent(dpy, &ev);
        switch (ev.type) {
            case Expose:
                if (ev.xexpose.count < 1)
                    paint_image(window_surface, image);
                break;
        }
    }

    cairo_surface_destroy(window_surface);
    XCloseDisplay(dpy);

    return 0;
}
