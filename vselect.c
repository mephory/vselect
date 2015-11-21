// Usage: vselect [OPTION]... FILENAME - select a rectangular area from an image
//
// Options:
//     -f FORMAT    specify an output format
//     -h           print this help
//
// FORMAT is any string that can contain the following vars:
//     %l / %x  left side
//     %t / %y  top side
//     %r       right side
//     %b       bottom side
//     %w       width
//     %h       height
//
// The default format is the default geometry syntax imagemagick uses:
//     %wx%h+%x+%y
//
// KEY AND MOUSE BINDINGS
//
// q            quit
// arrow up     zoom in
// arrow down   zoom out
// left mouse   select rectangle
// middle mouse move image
// right mouse  confirm and exit

// TODO:
//  - output in user-specified format
//  - display selections going out of bounds correctly
//  - display x,y,w,h while dragging the rectangle
//  - handle zoom+offset correctly (it's kind of weird right now)

#include <cairo.h>
#include <cairo-xlib.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DEFAULT_FORMAT "%wx%h+%x+%y"


// Type definitions
typedef struct {
    int x, y;
} point_t;

typedef struct {
    point_t start, end;
} rect_t;

typedef struct {
    char *format;
    char *filename;
} options_t;

typedef struct {
    rect_t sel;
    point_t offset;
    double zoom;
    int image_width;
    int image_height;
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

int getr(rect_t rect) {
    return max(rect.start.x, rect.end.x);
}

int getb(rect_t rect) {
    return max(rect.start.y, rect.end.y);
}

int getwidth(rect_t rect) {
    return abs(rect.end.x - rect.start.x);
}

int getheight(rect_t rect) {
    return abs(rect.end.y - rect.start.y);
}

char *itoa(int n) {
    char *str;
    int size = snprintf(NULL, 0, "%d", n);
    str = malloc(size);
    sprintf(str, "%d", n);
    return str;
}

char *str_replace(char *orig, char *rep, char *with) {
    char *result; // the return string
    char *ins;    // the next insert point
    char *tmp;    // varies
    int len_rep;  // length of rep
    int len_with; // length of with
    int len_front; // distance between rep and end of last rep
    int count;    // number of replacements

    if (!orig)
        return NULL;
    if (!rep)
        rep = "";
    len_rep = strlen(rep);
    if (!with)
        with = "";
    len_with = strlen(with);

    ins = orig;
    for (count = 0; tmp = strstr(ins, rep); ++count) {
        ins = tmp + len_rep;
    }

    // first time through the loop, all the variable are set correctly
    // from here on,
    //    tmp points to the end of the result string
    //    ins points to the next occurrence of rep in orig
    //    orig points to the remainder of orig after "end of rep"
    tmp = result = malloc(strlen(orig) + (len_with - len_rep) * count + 1);

    if (!result)
        return NULL;

    while (count--) {
        ins = strstr(orig, rep);
        len_front = ins - orig;
        tmp = strncpy(tmp, orig, len_front) + len_front;
        tmp = strcpy(tmp, with) + len_with;
        orig += len_front + len_rep; // move to next "end of rep"
    }
    strcpy(tmp, orig);
    return result;
}


// cairo stuff
cairo_status_t read_image(void *closure, unsigned char *data, unsigned int size) {
    FILE *f = (FILE *)closure;
    fread(data, size, 1, f);
    return CAIRO_STATUS_SUCCESS;
}

FILE *create_png_stream(char *filename) {
    char *command;
    int size = snprintf(NULL, 0, "convert %s png:- 2>/dev/null", filename);
    command = malloc(size);
    sprintf(command, "convert %s png:- 2>/dev/null", filename);

    FILE *f = popen(command, "r");
    return f;
}

cairo_surface_t *open_image(char *filename) {
    cairo_surface_t *surface;
    FILE *f = create_png_stream(filename);

    surface = cairo_image_surface_create_from_png_stream(read_image, f);

    if (cairo_surface_status(surface) == CAIRO_STATUS_FILE_NOT_FOUND
        || cairo_surface_status(surface) == CAIRO_STATUS_READ_ERROR
        || cairo_surface_status(surface) == CAIRO_STATUS_NO_MEMORY) {
        fprintf(stderr, "ERROR: Could not open file\n");
        exit(-1);
    }

    return surface;
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

void print_help() {
    printf("Usage: vselect [OPTION]... FILENAME - select a rectangular area from an image\n");
    printf("\n");
    printf("Options:\n");
    printf("    -f FORMAT    specify an output format\n");
    printf("    -h / --help  print this help\n");
    printf("\n");
    printf("FORMAT is any string that can contain the following vars:\n");
    printf("    %%l / %%x  left side\n");
    printf("    %%t / %%y  top side\n");
    printf("    %%r       right side\n");
    printf("    %%b       bottom side\n");
    printf("    %%w       width\n");
    printf("    %%h       height\n");
    printf("\n");
    printf("The default format is the default geometry syntax imagemagick uses:\n");
    printf("    %%wx%%h+%%x+%%y\n");
    printf("\n");
    printf("KEY AND MOUSE BINDINGS\n");
    printf("\n");
    printf("q            quit\n");
    printf("arrow up     zoom in\n");
    printf("arrow down   zoom out\n");
    printf("left mouse   select rectangle\n");
    printf("middle mouse move image\n");
    printf("right mouse  confirm and exit\n");
}

options_t parse_args(int argc, char **argv) {
    options_t opts;
    opts.format = DEFAULT_FORMAT;

    int format_flag_encountered = 0;

    for (int i = 1; i < argc; i++) {
        // if there -h or --help, then just print usage and exit
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_help();
            exit(0);
        }

        if (strcmp(argv[i], "-f") == 0) {
            format_flag_encountered = 1;
            continue;
        }

        if (format_flag_encountered) {
            opts.format = argv[i];
            format_flag_encountered = 0;
        } else {
            opts.filename = argv[i];
        }
    }

    if (strlen(opts.filename) == 0) {
        printf("ERROR: No filename given\n");
        exit(1);
    }

    return opts;
}

point_t to_image_point(int x, int y, state_t state) {
    point_t p;

    p.x = min(state.image_width, max(0, x * (1 / state.zoom) - state.offset.x));
    p.y = min(state.image_height, max(0, y * (1 / state.zoom) - state.offset.y));

    return p;
}

void output(state_t state, options_t options) {
    // TODO: Don't assume that's enough
    char *output = malloc(5000);

    strcpy(output, options.format);
    output = str_replace(output, "\%x", itoa(getx(state.sel)));
    output = str_replace(output, "\%y", itoa(gety(state.sel)));
    output = str_replace(output, "\%l", itoa(getx(state.sel)));
    output = str_replace(output, "\%t", itoa(gety(state.sel)));
    output = str_replace(output, "\%r", itoa(getr(state.sel)));
    output = str_replace(output, "\%b", itoa(getb(state.sel)));
    output = str_replace(output, "\%w", itoa(getwidth(state.sel)));
    output = str_replace(output, "\%h", itoa(getheight(state.sel)));

    printf("%s\n", output);
    fflush(stdout);
}

int main(int argc, char **argv) {
    XEvent ev;
    Window win;
    cairo_surface_t *window_surface;
    cairo_surface_t *image;
    options_t options;
    state_t state;

    // create initial state
    state.zoom = 1.0;
    state.offset.x = 0;
    state.offset.y = 0;
    state.sel.start.x = 0;
    state.sel.start.y = 0;
    state.sel.end.x = 0;
    state.sel.end.y = 0;

    // read command line arguments
    options = parse_args(argc, argv);

    // open the image and get the dimensions
    image = open_image(options.filename);
    state.image_width = cairo_image_surface_get_width(image);
    state.image_height = cairo_image_surface_get_height(image);

    // initialize and map x window with appropriate size
    initialize_xlib();
    win = create_window("vselect", state.image_width, state.image_height);
    XMapWindow(dpy, win);
    window_surface = cairo_xlib_surface_create(dpy, win,
        DefaultVisual(dpy, 0), state.image_width + 1, state.image_height + 1);


    int drag_start_x, drag_start_y;
    int drag_last_x, drag_last_y;
    int break_from_mainloop = 0;

    while (!break_from_mainloop) {
        while (XPending(dpy) > 0) {
            XNextEvent(dpy, &ev);
            switch (ev.type) {
                case ConfigureNotify:
                    cairo_xlib_surface_set_size(window_surface, ev.xconfigure.width, ev.xconfigure.height);
                    break;
                case KeyPress:
                    switch (XLookupKeysym(&ev.xkey, 0)) {
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
                    if (ev.xbutton.button == 3) {
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
                        // Button 3: adjust offset
                        state.offset.x += ev.xmotion.x - drag_last_x;
                        state.offset.y += ev.xmotion.y - drag_last_y;
                    }

                    drag_last_x = ev.xmotion.x;
                    drag_last_y = ev.xmotion.y;
                    break;
            }
        }
        usleep(25000);
        paint(window_surface, image, state);
    }

    output(state, options);

    exit(0);
}
