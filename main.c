#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "util.h"

// function declarations
void configurerequest(XEvent * e);
void configurenotify(XEvent * e);
void destroynotify(XEvent * e);
void maprequest(XEvent * e);
void mapnotify(XEvent * e);
void unmapnotify(XEvent * e);
void keypress(XEvent * e);

int border_width = 4;
Display *dpy;
Window root;
int root_width;
int root_height;
void (*handler[LASTEvent]) (XEvent *) = {
	[ConfigureRequest] = configurerequest,
	[ConfigureNotify] = configurenotify,
	[DestroyNotify] = destroynotify,
	[MapRequest] = maprequest,
	[MapNotify] = mapnotify,
	[UnmapNotify] = unmapnotify,
	[KeyPress] = keypress,
};
Cursor cursor;

// function definitions
void configurerequest(XEvent * e) {
	XConfigureRequestEvent *ev = &e->xconfigurerequest;

	XWindowChanges changes;
	changes.x = ev->x;
	changes.y = ev->y;
	changes.width = ev->width;
	changes.height = ev->height;
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
}

void unmapnotify(XEvent * e) {
	XUnmapEvent *ev = &e->xunmap;
	printf("A window %lu has been unmapped\n", ev->window);
}
void keypress(XEvent * e) {
	XKeyPressedEvent *ev = &e->xkey;
	printf("A key %d has been pressed\n", ev->keycode);
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
}

void run() {
	XEvent ev;
	XSync(dpy, False);
	for (;;) {
		XNextEvent(dpy, &ev);
		if (handler[ev.type]) {
			handler[ev.type](&ev);
		}
	}
}

int main() {
	setup();
	run();
	return EXIT_SUCCESS;
}
