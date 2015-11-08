// Usage: vselect [OPTION]... FILENAME - select a rectangular area from an image
//
// Options:
//     -f, --format=FORMAT specify an output format
//
// FILENAME must be an image file or - for stdin
//
// FORMAT is any string that can contain the following vars:
//     %x      left side
//     %y      top side
//     %x2     right side
//     %y2     bottom side
//     %w      width
//     %h      height
//
// The default format is the default geometry syntax imagemagick uses:
//     +%x+%y          for points
//     %wx%h+%x+%y     for rects
//
// KEY AND MOUSE BINDINGS
//
// q            quit
// arrow up     zoom in
// arrow down   zoom out
// left mouse   select rectangle
// middle mouse zoom in/out
// right mouse  move image

// TODO:
//  - parse arguments
//  - load image from arguments (and support more than just png)
//  - output in user-specified format
//  - handle zoom+offset correctly (it's kind of weird right now)
//  - display selections going out of bounds correctly
//  - display x,y,w,h while dragging the rectangle
//  - decide when to exit the program (confirmation?)

#include <cairo.h>
#include <cairo-xlib.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <stdio.h>
#include <stdlib.h>

// Type definitions
typedef struct {
    int x, y;
} point_t;

typedef struct {
    point_t start, end;
} rect_t;

typedef struct {
    int select_point;
    char *filename;
} options_t;

typedef struct {
    rect_t sel;
    point_t offset;
    double zoom;
} state_t;


// Global variables
Display *dpy;
int screen;
Window root;


// Xlib stuff
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
    XSelectInput(dpy, win,
        ExposureMask | ButtonPressMask | ButtonReleaseMask | ButtonMotionMask | KeyPressMask |
        StructureNotifyMask);

    return win;
}

KeySym get_keysym(XKeyEvent ev) {
    char buffer[20];
    KeySym key;

    return XLookupKeysym(&ev, 0);
}


// Utils
int min(int a, int b) {
    return (a < b) ? a : b;
}

int max(int a, int b) {
    return (a > b) ? a : b;
}

int abs(int n) {
    return (n < 0) ? (-1) * n : n;
}

int getx(rect_t rect) {
    return min(rect.start.x, rect.end.x);
}

int gety(rect_t rect) {
    return min(rect.start.y, rect.end.y);
}

int getwidth(rect_t rect) {
    return abs(rect.end.x - rect.start.x);
}

int getheight(rect_t rect) {
    return abs(rect.end.y - rect.start.y);
}



cairo_surface_t *open_image(char *filename) {
    return cairo_image_surface_create_from_png(filename);
}

void paint(cairo_surface_t *window, cairo_surface_t *image, state_t state) {
    cairo_t *c;

    c = cairo_create(window);

    // highlighted area
    cairo_surface_t *highlighted = cairo_surface_create_for_rectangle(
        image, getx(state.sel), gety(state.sel), getwidth(state.sel), getheight(state.sel));

    // draw to second buffer
    cairo_push_group(c);

    // zoom
    cairo_scale(c, state.zoom, state.zoom);

    // clear screen
    cairo_set_source_rgba(c, 0, 0, 0, 1);
    cairo_paint(c);

    // draw grayed out image
    cairo_set_source_surface(c, image, state.offset.x, state.offset.y);
    cairo_paint_with_alpha(c, .25);

    // draw highlighted part
    cairo_set_source_surface(c, highlighted,
        getx(state.sel) + state.offset.x, gety(state.sel) + state.offset.y);
    cairo_paint(c);

    // draw to actual buffer
    cairo_pop_group_to_source(c);
    cairo_paint(c);

    // free memory
    cairo_destroy(c);

}

options_t parse_args(int argc, char **argv) {
    options_t opts;

    opts.select_point = 1;
    opts.filename = "./test.png";
    return opts;
}

/* int to_image_xy(int orig_xy, double zoom_factor, int offset) { */
/*     return orig_xy * (1 / zoom_factor) - offset; */
/* } */
point_t to_image_point(int x, int y, state_t state) {
    point_t p;

    p.x = max(0, x * (1 / state.zoom) - state.offset.x);
    p.y = max(0, y * (1 / state.zoom) - state.offset.y);

    return p;
}

int main(int argc, char **argv) {
    XEvent ev;
    Window win;
    cairo_surface_t *window_surface;
    cairo_surface_t *image;
    int width, height;
    options_t options;
    state_t state;

    // create initial state
    state.zoom = 1.0;
    state.offset.x = 0;
    state.offset.y = 0;

    // read command line arguments
    options = parse_args(argc, argv);

    // open the image and get the dimensions
    image = open_image(options.filename);
    width = cairo_image_surface_get_width(image);
    height = cairo_image_surface_get_height(image);

    // initialize and map x window with appropriate size
    initialize_xlib();
    win = create_window("vselect", width, height);
    XMapWindow(dpy, win);
    window_surface = cairo_xlib_surface_create(dpy, win,
        DefaultVisual(dpy, 0), width + 1, height + 1);


    int drag_start_x, drag_start_y;
    int drag_last_x, drag_last_y;
    int break_from_mainloop = 0;

    while (!break_from_mainloop) {
        XNextEvent(dpy, &ev);
        switch (ev.type) {
            case Expose:
                if (ev.xexpose.count < 1)
                    paint(window_surface, image, state);
                break;
            case ConfigureNotify:
                cairo_xlib_surface_set_size(window_surface, ev.xconfigure.width, ev.xconfigure.height);
                break;
            case KeyPress:
                switch (get_keysym(ev.xkey)) {
                    // Press 'q' to quit
                    case XK_q:
                        exit(0);
                        break;
                    // Press 'Up' to zoom in
                    case XK_Up:
                        state.zoom += .1;
                        break;
                    // Press 'Down' to zoom out
                    case XK_Down:
                        state.zoom -= .1;
                        break;
                }
                break;
            case ButtonPress:
                drag_start_x = ev.xbutton.x;
                drag_start_y = ev.xbutton.y;
                drag_last_x = ev.xbutton.x;
                drag_last_y = ev.xbutton.y;
                break;
            case ButtonRelease:
                if (ev.xbutton.button == 1) {
                    if (ev.xbutton.x == drag_start_x && ev.xbutton.y == drag_start_y)
                        break_from_mainloop = 1;
                }
                break;
            case MotionNotify:
                if (ev.xmotion.state & Button1Mask) {
                    // Button 1: make selection
                    state.sel.start = to_image_point(drag_start_x, drag_start_y, state);
                    state.sel.end = to_image_point(ev.xmotion.x, ev.xmotion.y, state);
                }
                if (ev.xmotion.state & Button2Mask) {
                    // Button 2: adjust zoom
                    state.zoom += .1 * (ev.xmotion.y - drag_last_y);
                }
                if (ev.xmotion.state & Button3Mask) {
                    // Button 3: adjust offset
                    state.offset.x += ev.xmotion.x - drag_last_x;
                    state.offset.y += ev.xmotion.y - drag_last_y;
                }

                drag_last_x = ev.xmotion.x;
                drag_last_y = ev.xmotion.y;
                break;
        }

        if (ev.type != 65)
            paint(window_surface, image, state);

        fflush(stdout);
    }

    printf("%dx%d+%d+%d",
        getwidth(state.sel),
        getheight(state.sel),
        getx(state.sel),
        gety(state.sel));

    exit(0);
}
