#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/Xft/Xft.h>
#include <X11/cursorfont.h>
#include <fontconfig/fontconfig.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "util.h"

typedef struct Client Client;
struct Client {
	char name[64];
	Window wnd;
	Client *prev;
	Client *next;
};

// function declarations
void configurerequest(XEvent * e);
void configurenotify(XEvent * e);
void destroynotify(XEvent * e);
void maprequest(XEvent * e);
void mapnotify(XEvent * e);
void unmapnotify(XEvent * e);
void keypress(XEvent * e);
void expose(XEvent * e);
void focus(Client *c);
void unfocus(Client *c);
void manage(Window w);
void unmanage(Window w);
void spawn(const char *a);
void init();
void createbar();
void updatebar();
void drawbar();

const int border_width = 0;
const int bar_height = 32;
const int bar_padding = 10;
Display *dpy;
Window root;
int root_width;
int root_height;
Window statusbar;
//"Iosevka Nerd Font Mono:size=15", "monospace:size=15"
const char *fontname = "Iosevka Nerd Font Mono:size=20";
XftFont *font;
Visual *visual;
Colormap colormap;
XftDraw *draw;
static const char *fg_color_const = "#e4e4ef";
XftColor fg_color;
static const char *bg_color_const = "#2e3440";
XftColor bg_color;
static const char *primary_color_const = "#88c0d0";
XftColor primary_color;
void (*handler[LASTEvent]) (XEvent *) = {
	[ConfigureRequest] = configurerequest,
	[ConfigureNotify] = configurenotify,
	[DestroyNotify] = destroynotify,
	[MapRequest] = maprequest,
	[MapNotify] = mapnotify,
	[UnmapNotify] = unmapnotify,
	[KeyPress] = keypress,
	[Expose] = expose,
};
Cursor cursor;
Client *clients = NULL;
Client *focused = NULL;

// function definitions
void spawn(const char *cmd) {
    if (fork() == 0) {
        if (dpy) {
            close(ConnectionNumber(dpy));
        }
        setsid();
        execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
        fprintf(stderr, "execl failed: %s\n", cmd);
        exit(EXIT_FAILURE);
    }
}

void init() {
	system("dunst");
	system("nitrogen --restore");
	system("xsetroot -cursor_name left_ptr");
	system("setxkbmap -layout us,cz -option grp:alt_shift_toggle");
	system("xmodmap -e 'clear lock'");
	system("xmodmap -e 'keycode 9 = Escape NoSymbol Escape'");
	system("xmodmap -e 'keycode 66 = Escape NoSymbol Escape'");
}

void loadfont() {
	font = XftFontOpen(dpy, 0, fontname, NULL);
	if (font == NULL) {
		font = XftFontOpenName(dpy, 0, "monospace:size=15");
		if (font == NULL) {
			panic("Could not load font...");
		}
	}
}

void createbar() {
	statusbar = XCreateSimpleWindow(dpy, root, 0, 0, root_width, bar_height, 0, 0, 0);
	XSelectInput(dpy, statusbar, ExposureMask);
	XSetWindowAttributes attr;
	attr.override_redirect = True;
	XChangeWindowAttributes(dpy, statusbar, CWOverrideRedirect, &attr);
	XMapWindow(dpy, statusbar);

	loadfont();

	visual = DefaultVisual(dpy, 0);
	colormap = DefaultColormap(dpy, 0);
	XftColorAllocName(dpy, visual, colormap, fg_color_const, &fg_color);
	XftColorAllocName(dpy, visual, colormap, bg_color_const, &bg_color);
	XftColorAllocName(dpy, visual, colormap, primary_color_const, &primary_color);
	draw = XftDrawCreate(dpy, statusbar, visual, colormap);
}

void updatebar() {
	XClearWindow(dpy, statusbar);
	XftDrawRect(draw, &bg_color, 0, 0, root_width, bar_height);

	if (clients == NULL) {
		const char *msg = "No clients";
		XftDrawString8(draw, &fg_color, font, bar_padding, bar_height - 8, (const FcChar8*)msg, strlen(msg));
	}

	int width = 0;
	for (Client *c = clients; c != NULL; c = c->next) {
		printf("Drawing %lu\n", c->wnd);
		XGlyphInfo extents;
		XftTextExtents8(dpy, font, (const FcChar8*)c->name, strlen(c->name), &extents);

		if (c == focused) {
			XftDrawRect(draw, &primary_color, width, 0, extents.width+2*bar_padding, bar_height);
			XftDrawString8(draw, &bg_color, font, width+bar_padding, bar_height - 8, (const FcChar8*)c->name, strlen(c->name));
		} else {
			XftDrawString8(draw, &fg_color, font, width+bar_padding, bar_height - 8, (const FcChar8*)c->name, strlen(c->name));
		}
		width += extents.width + bar_padding*2;
	}

	XSync(dpy, False);
}

void configurerequest(XEvent * e) {
	XConfigureRequestEvent *ev = &e->xconfigurerequest;

	XWindowChanges changes;
	changes.x = 0;
	changes.y = bar_height;
	changes.width = root_width;
	changes.height = root_height - bar_height;
	changes.border_width = border_width;
	changes.stack_mode = Above;

	XConfigureWindow(dpy, ev->window, CWX|CWY|CWWidth|CWHeight|CWBorderWidth|CWStackMode, &changes);
}

void configurenotify(XEvent * e) {
	XConfigureEvent *ev = &e->xconfigure;
	printf("A window %lu has been configured\n", ev->window);
}

void destroynotify(XEvent * e) {
	XDestroyWindowEvent *ev = &e->xdestroywindow;
	printf("A window %lu has been destroyed\n", ev->window);
}

void maprequest(XEvent * e) {
	XMapRequestEvent *ev = &e->xmaprequest;
	XMapWindow(dpy, ev->window);
}
void mapnotify(XEvent * e) {
	XMapEvent *ev = &e->xmap;
	printf("A window %lu has been mapped\n", ev->window);
	if (!ev->override_redirect) {
		manage(ev->window);
	}
}

void unmapnotify(XEvent * e) {
	XUnmapEvent *ev = &e->xunmap;
	printf("A window %lu has been unmapped\n", ev->window);
	unmanage(ev->window);
}
void keypress(XEvent * e) {
	XKeyPressedEvent *ev = &e->xkey;
	printf("A key %d has been pressed\n", ev->keycode);

	if (ev->keycode == XKeysymToKeycode(dpy, XK_Return) && ev->state == Mod4Mask) {
		spawn("alacritty");
	}

	if (ev->keycode == XKeysymToKeycode(dpy, XK_space) && ev->state == Mod4Mask) {
		spawn("dmenu_run");
	}

	if (ev->keycode == XKeysymToKeycode(dpy, XK_q) && ev->state == Mod4Mask) {
		if (focused != NULL) {
			XDestroyWindow(dpy, focused->wnd);

			// unmanage(focused->wnd);
			// leads to double free, gotta think if we want to do it like this
			// ie have some additional behavior...
		}
	}

	if (ev->keycode == XKeysymToKeycode(dpy, XK_k) && ev->state == Mod4Mask) {
		if (focused != NULL) {
			focus(focused->prev);
		}
	}

	if (ev->keycode == XKeysymToKeycode(dpy, XK_l) && ev->state == Mod4Mask) {
		if (focused != NULL) {
			focus(focused->next);
		}
	}
}

void expose(XEvent * e) {
	XExposeEvent *ev = &e->xexpose;
	printf("An expose event has been triggered\n");
	
	if (ev->window == statusbar) {
		updatebar();
	}
}

void focus(Client *c) {
	if (c == NULL) {
		return;
	}
	printf("focusing %lu\n", c->wnd);
	focused = c;
	XSetInputFocus(dpy, c->wnd, RevertToPointerRoot, CurrentTime);
	XRaiseWindow(dpy, c->wnd);
	XSync(dpy, False);
	updatebar();
}

void unfocus(Client *c) {
	if (c == NULL) {
		return;
	}
	focused = NULL;
	printf("focusing root\n");
	XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
	XSync(dpy, False);
	updatebar();
}

Client *wintoclient(Window wnd) {
	Client *cc = clients;
	while (cc != NULL && cc->wnd != wnd) {
		cc = cc->next;
	}
	return cc;
}

void manage(Window wnd) {
	Client *c = malloc(sizeof(Client));
	c->wnd = wnd;

	XTextProperty name;
	XGetTextProperty(dpy, c->wnd, &name, XA_WM_NAME);

	if (name.encoding == XA_STRING) {
		strncpy(c->name,(char *)name.value, sizeof(c->name));
	}

	if (clients == NULL) {
		c->next = NULL;
		c->prev = NULL;
		clients = c;
	} else {
		Client *cc = clients;
		while (cc->next != NULL) {
			cc = cc-> next;
		}
		c->prev = cc;
		c->next = NULL;
		cc->next = c;
	}

	focus(c);
	printf("Managing %lu:%s\n", c->wnd, c->name);

	printf("updating from manage\n");
	updatebar();
}

void unmanage(Window wnd) {
	Client *c = wintoclient(wnd);
	if (c == NULL) {
		return;
	}

	if (c->next != NULL) {
		if (c->prev != NULL) {
			c->next->prev = c->prev;
		} else {
			c->next->prev = NULL;
		}
	}

	if (c->prev != NULL) {
		if (c->next != NULL) {
			c->prev->next = c->next;
		} else {
			c->prev->next = NULL;
		}
	}

	if (focused == c) {
		unfocus(c);

		if (c->prev != NULL) {
			printf("focusing prev\n");
			focus(c->prev);
		} else if (c->next != NULL) {
			printf("focusing next\n");
			focus(c->next);
		}
	}

	if (c->prev == NULL) {
		if (c->next == NULL) {
			clients = NULL;
		} else {
			clients = c->next;
		}
	}

	printf("Unmanaging %lu\n", c->wnd);
	free(c);

	printf("updating from unmanage\n");
	updatebar();
}

void grabkeys() {
	XGrabKey(dpy, XKeysymToKeycode(dpy, XK_Return), Mod4Mask, root, True, GrabModeAsync, GrabModeAsync);
	XGrabKey(dpy, XKeysymToKeycode(dpy, XK_space), Mod4Mask, root, True, GrabModeAsync, GrabModeAsync);
	XGrabKey(dpy, XKeysymToKeycode(dpy, XK_q), Mod4Mask, root, True, GrabModeAsync, GrabModeAsync);
	XGrabKey(dpy, XKeysymToKeycode(dpy, XK_k), Mod4Mask, root, True, GrabModeAsync, GrabModeAsync);
	XGrabKey(dpy, XKeysymToKeycode(dpy, XK_l), Mod4Mask, root, True, GrabModeAsync, GrabModeAsync);
}

void setup() {
	dpy = XOpenDisplay(NULL);
	if (dpy == NULL) {
		panic("Could not open display...");
	}

	root = DefaultRootWindow(dpy);
	root_width = DisplayWidth(dpy, 0);
	root_height = DisplayHeight(dpy, 0);

	XSelectInput(dpy, root, SubstructureRedirectMask|SubstructureNotifyMask);
	XSync(dpy, False);

	cursor = XCreateFontCursor(dpy, XC_X_cursor);
	XDefineCursor(dpy, root, cursor);
	XSync(dpy, False);

	createbar();
	updatebar();

	grabkeys();

	init();
}

void run() {
	XEvent ev;
	XSync(dpy, False);
	for (;;) {
		XNextEvent(dpy, &ev);
		if (handler[ev.type]) {
			handler[ev.type](&ev);
		} else {
			printf("No handler for event %d\n", ev.type);
		}
	}
}

int main() {
	setup();
	run();
	return EXIT_SUCCESS;
}
