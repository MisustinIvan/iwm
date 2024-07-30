#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/Xft/Xft.h>
#include <X11/cursorfont.h>
#include <X11/extensions/Xrender.h>
#include <fontconfig/fontconfig.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "util.h"
#include "iwm.h"

// function declarations
// XEvent handlers
void configurerequest(XEvent *e);
void configurenotify(XEvent *e);
void destroynotify(XEvent *e);
void maprequest(XEvent *e);
void mapnotify(XEvent *e);
void unmapnotify(XEvent *e);
void keypress(XEvent *e);
void expose(XEvent *e);
void propertynotify(XEvent *e);
// window management
void focus(Client *c);
void unfocus(Client *c);
void manage(Window w);
void unmanage(Window w);
Client *wintoclient(Window wnd);
void updatewm();
void updatetitle(Client *c);
// bar functions
Bar createbar(int width, int height, int posx, int posy);
void updatestatus(Bar *b);
void updatebar(Bar *b);
void togglebar(Bar *b);
void setbar(Bar *b, Bool arg);
// wm utilities
void spawn(const char *a);
void init();
void scan();
void quit(Bool arg);
void sighup();
void sigterm();

// global variables
// state
Bool running = True;
Bool restart = False;
Bool bar = True;
// consts
const int border_width = 0;
#define BAR_HEIGHT 35
#define BAR_BORDER_WIDTH 2
#define BAR_PADDING 10
// wm stuff
Display *dpy;
Window root;
int root_width;
int root_height;
Client *clients = NULL;
Client *focused = NULL;
Cursor cursor;
// XEvent handler
void (*handler[LASTEvent]) (XEvent *) = {
	[ConfigureRequest] = configurerequest,
	[ConfigureNotify] = configurenotify,
	[DestroyNotify] = destroynotify,
	[MapRequest] = maprequest,
	[MapNotify] = mapnotify,
	[UnmapNotify] = unmapnotify,
	[KeyPress] = keypress,
	[Expose] = expose,
	[PropertyNotify] = propertynotify,
};
// bar variables
Bar statusbar;
// Window statusbar;
// char status[128];
const char *fontname = "Iosevka Nerd Font Mono:size=15";
// XftFont *font;
// Visual *visual;
// Colormap colormap;
// XftDraw *draw;
static const char *fg_color_const = "#e4e4ef";
// XftColor fg_color;
static const char *bg_color_const = "#2e3440";
// XftColor bg_color;
static const char *primary_color_const = "#88c0d0";
// XftColor primary_color;

// function definitions
void spawn(const char *cmd) {
    if (fork() == 0) {
        if (dpy) {
            close(ConnectionNumber(dpy));
        }
        setsid();
        execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
#ifdef DEBUG
        fprintf(stderr, "execl failed: %s\n", cmd);
#endif
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

void loadfont(XftFont **ft, const char * fontname) {
	*ft = XftFontOpenName(dpy, 0, fontname);
	if (*ft == NULL) {
#ifdef DEBUG
		printf("Could not load set font\n");
#endif
		*ft = XftFontOpenName(dpy, 0, "monospace:size=15");
		if (*ft == NULL) {
			panic("Could not load font...");
		}
	}
}

Bar createbar(int width, int height, int posx, int posy) {
	Bar b;
	// create the bar window
	b.wnd = XCreateSimpleWindow(dpy, root, posx, posy, width, height, 0, 0, 0);
	// receive expose events to redraw the bar
	XSelectInput(dpy, b.wnd, ExposureMask);
	// set override redirect so that we dont manage our own window
	XSetWindowAttributes attr;
	attr.override_redirect = True;
	XChangeWindowAttributes(dpy, b.wnd, CWOverrideRedirect, &attr);
	// map the window
	XMapWindow(dpy, b.wnd);
	// load the font
	loadfont(&b.font, fontname);
	// init the drawing context
	b.visual = DefaultVisual(dpy, 0);
	b.colormap = DefaultColormap(dpy, 0);
	XftColorAllocName(dpy, b.visual, b.colormap, fg_color_const, &b.fg_color);
	XftColorAllocName(dpy, b.visual, b.colormap, bg_color_const, &b.bg_color);
	XftColorAllocName(dpy, b.visual, b.colormap, primary_color_const, &b.primary_color);
	b.draw = XftDrawCreate(dpy, b.wnd, b.visual, b.colormap);
	// fill out the remaining fields
	b.width = width;
	b.height = height;
	b.posx = posx;
	b.posy = posy;
	b.border = BAR_BORDER_WIDTH;
	b.padding = BAR_PADDING;

	return b;
}

void updatestatus(Bar *b) {
	XTextProperty s;
	XGetTextProperty(dpy, root, &s, XA_WM_NAME);

	if (s.encoding == XA_STRING && s.value != NULL) {
		strcpy(b->status, (char *)s.value);
	} else {
		strcpy(b->status, "IWM");
	}
}

void updatebar(Bar *b) {
	if (b == NULL) {
		return;
	}

	XClearWindow(dpy, b->wnd);
	XftDrawRect(b->draw, &b->bg_color, 0, 0, b->width, b->height);

	updatestatus(b);

	if (clients == NULL) {
		const char *msg = "No clients";

		XftDrawStringUtf8(b->draw, &b->fg_color, b->font, b->padding, b->height - 8, (const FcChar8*)msg, strlen(msg));
	}

	// draw status
	XGlyphInfo status_extents;
	XftTextExtents8(dpy, b->font, (const FcChar8*)b->status, strlen(b->status), &status_extents);

	XftDrawRect(b->draw,    &b->primary_color,        b->posx + b->width - status_extents.width - 2*b->border - 2*b->padding, 0, status_extents.width + 2*b->padding + 2*b->border, b->height);
	XftDrawRect(b->draw,    &b->bg_color,             b->posx + b->width - status_extents.width - b->border - 2*b->padding, b->border, status_extents.width + 2*b->padding, b->height - 2*b->border);
	XftDrawString8(b->draw, &b->fg_color, b->font,    b->posx + b->width - status_extents.width - b->border - b->padding, b->height - 8, (const FcChar8*)b->status, strlen(b->status));

	//int width = 0;
	int width = b->posx;
	for (Client *c = clients; c != NULL; c = c->next) {
		// updatetitle(c);
	
#ifdef DEBUG
		printf("Drawing %lu\n", c->wnd);
#endif
		XGlyphInfo extents;
		XftTextExtents8(dpy, b->font, (const FcChar8*)c->name, strlen(c->name), &extents);

		if (c == focused) {
			XftDrawRect(b->draw, &b->primary_color, width, 0, extents.width + 2*b->padding, b->height);
			XftDrawString8(b->draw, &b->bg_color, b->font, width+b->padding, b->height - 8, (const FcChar8*)c->name, strlen(c->name));
		} else {
			XftDrawRect(b->draw, &b->primary_color, width, 0, extents.width + 2*b->padding, b->height);
			XftDrawRect(b->draw, &b->bg_color, width+b->border, b->border, extents.width + 2*b->padding - 2*b->border, b->height - 2*b->border);
			XftDrawString8(b->draw, &b->fg_color, b->font, width+b->padding, b->height - 8, (const FcChar8*)c->name, strlen(c->name));
		}
		width += extents.width + b->padding*2;
	}

	XSync(dpy, False);
}

void updatewm() {
	Client *c = clients;
	XWindowChanges changes;
	while (c != NULL) {
		if (bar) {
			changes.height = root_height - statusbar.height;
			changes.y = statusbar.height;
		} else {
			changes.height = root_height;
			changes.y = 0;
		}
	
		XConfigureWindow(dpy, c->wnd, CWHeight|CWY, &changes);

		c = c->next;
	}
}

void togglebar(Bar *b) {
	setbar(b, !bar);
}

void setbar(Bar *b, Bool arg) {
	bar = arg;
	updatebar(b);
	updatewm();
}

void configurerequest(XEvent * e) {
	XConfigureRequestEvent *ev = &e->xconfigurerequest;

	XWindowChanges changes;
	changes.x = 0;
	changes.y = statusbar.height;
	changes.width = root_width;
	changes.height = root_height - statusbar.height;
	changes.border_width = border_width;
	changes.stack_mode = Above;

	XConfigureWindow(dpy, ev->window, CWX|CWY|CWWidth|CWHeight|CWBorderWidth|CWStackMode, &changes);
}

void configurenotify(XEvent * e) {
	XConfigureEvent *ev = &e->xconfigure;
#ifdef DEBUG
	printf("A window %lu has been configured\n", ev->window);
#endif
}

void destroynotify(XEvent * e) {
	XDestroyWindowEvent *ev = &e->xdestroywindow;
#ifdef DEBUG
	printf("A window %lu has been destroyed\n", ev->window);
#endif
}

void maprequest(XEvent * e) {
	XMapRequestEvent *ev = &e->xmaprequest;
	XMapWindow(dpy, ev->window);
}
void mapnotify(XEvent * e) {
	XMapEvent *ev = &e->xmap;
#ifdef DEBUG
	printf("A window %lu has been mapped\n", ev->window);
#endif
	if (!ev->override_redirect) {
		manage(ev->window);
	}
}

void unmapnotify(XEvent * e) {
	XUnmapEvent *ev = &e->xunmap;
#ifdef DEBUG
	printf("A window %lu has been unmapped\n", ev->window);
#endif
	unmanage(ev->window);
}
void keypress(XEvent * e) {
	XKeyPressedEvent *ev = &e->xkey;
#ifdef DEBUG
	printf("A key %d has been pressed\n", ev->keycode);
#endif

	if (ev->keycode == XKeysymToKeycode(dpy, XK_Return) && ev->state == Mod4Mask) {
		spawn("alacritty");
	}

	if (ev->keycode == XKeysymToKeycode(dpy, XK_space) && ev->state == Mod4Mask) {
		spawn("dmenu_run");
	}

	if (ev->keycode == XKeysymToKeycode(dpy, XK_r) && ev->state == (Mod4Mask|ControlMask)) {
		quit(True);
	}

	if (ev->keycode == XKeysymToKeycode(dpy, XK_q) && ev->state == (Mod4Mask|ControlMask|ShiftMask)) {
		quit(False);
	}

	if (ev->keycode == XKeysymToKeycode(dpy, XK_q) && ev->state == Mod4Mask) {
		if (focused != NULL) {
			XDestroyWindow(dpy, focused->wnd);
		}
	}

	if (ev->keycode == XKeysymToKeycode(dpy, XK_b) && ev->state == Mod4Mask) {
		togglebar(&statusbar);
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

	if (ev->keycode == XKeysymToKeycode(dpy, XK_k) && ev->state == (Mod4Mask|ShiftMask)) {
		if (focused != NULL) {
			if (focused->prev != NULL) {
				// Client *focused = focused;
				Client *other = focused->prev;

				Client *left = focused->prev->prev;
				Client *right = focused->next;

				if (left != NULL) left->next = focused;
				focused->prev = left;
				if (right != NULL) right->prev = other;
				other->next = right;

				focused->next = other;
				other->prev = focused;

				if (other == clients) clients = focused;

				updatebar(&statusbar);
			}
		}
	}

	if (ev->keycode == XKeysymToKeycode(dpy, XK_l) && ev->state == (Mod4Mask|ShiftMask)) {
		if (focused != NULL) {
			if (focused->next != NULL) {
				// Client *focused = focused;
				Client *other = focused->next;

				Client *left = focused->prev;
				Client *right = other->next;

				if (right != NULL) right->prev = focused;
				focused->next = right;
				if (left != NULL) left->next = other;
				other->prev = left;

				focused->prev = other;
				other->next = focused;

				if (focused == clients) clients = other;

				updatebar(&statusbar);
			}
		}
	}
}

void expose(XEvent * e) {
	XExposeEvent *ev = &e->xexpose;
#ifdef DEBUG
	printf("An expose event has been triggered\n");
#endif
	
	if (ev->window == statusbar.wnd) {
		updatebar(&statusbar);
	}
}

void propertynotify(XEvent *e) {
	XPropertyEvent *ev = &e->xproperty;

	if (ev->window == root) {
		updatebar(&statusbar);
	}

	if (ev->atom == XA_WM_NAME) {
		Client *c = wintoclient(ev->window);
		if (c != NULL) {
			updatetitle(c);
			updatebar(&statusbar);
		}
	}
}

void updatetitle(Client *c) {
	XTextProperty name;
	XGetTextProperty(dpy, c->wnd, &name, XA_WM_NAME);

	if (name.encoding == XA_STRING && name.value != NULL) {
		strncpy(c->name,(char *)name.value, sizeof(c->name));
	}
}

void focus(Client *c) {
	if (c == NULL) {
		return;
	}
#ifdef DEBUG
	printf("focusing %lu\n", c->wnd);
#endif
	focused = c;
	XSetInputFocus(dpy, c->wnd, RevertToPointerRoot, CurrentTime);
	XRaiseWindow(dpy, c->wnd);
	XSync(dpy, False);
	updatebar(&statusbar);
}

void unfocus(Client *c) {
	if (c == NULL) {
		return;
	}
	focused = NULL;
#ifdef DEBUG
	printf("focusing root\n");
#endif
	XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
	XSync(dpy, False);
	updatebar(&statusbar);
}

Client *wintoclient(Window wnd) {
	Client *cc = clients;
	while (cc != NULL && cc->wnd != wnd) {
		cc = cc->next;
	}
	return cc;
}

// register a window with the window manager
void manage(Window wnd) {
	Client *c = malloc(sizeof(Client));
	c->wnd = wnd;

	XSelectInput(dpy, c->wnd, PropertyChangeMask);
	XSync(dpy, False);

	updatetitle(c);

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
#ifdef DEBUG
	printf("Managing %lu:%s\n", c->wnd, c->name);
#endif

	XWindowChanges changes;
	changes.x = 0;
	changes.y = statusbar.height;
	changes.width = root_width;
	changes.height = root_height - statusbar.height;
	changes.border_width = border_width;
	changes.stack_mode = Above;

	XConfigureWindow(dpy, c->wnd, CWX|CWY|CWWidth|CWHeight|CWBorderWidth|CWStackMode, &changes);

#ifdef DEBUG
	printf("updating from manage\n");
#endif
	updatebar(&statusbar);
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
#ifdef DEBUG
			printf("focusing prev\n");
#endif
			focus(c->prev);
		} else if (c->next != NULL) {
#ifdef DEBUG
			printf("focusing next\n");
#endif
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

#ifdef DEBUG
	printf("Unmanaging %lu\n", c->wnd);
#endif
	free(c);

#ifdef DEBUG
	printf("updating from unmanage\n");
#endif
	updatebar(&statusbar);
}

void grabkeys() {
	XGrabKey(dpy, XKeysymToKeycode(dpy, XK_Return), Mod4Mask, root, True, GrabModeAsync, GrabModeAsync);
	XGrabKey(dpy, XKeysymToKeycode(dpy, XK_space), Mod4Mask, root, True, GrabModeAsync, GrabModeAsync);
	XGrabKey(dpy, XKeysymToKeycode(dpy, XK_r), Mod4Mask|ControlMask, root, True, GrabModeAsync, GrabModeAsync);
	XGrabKey(dpy, XKeysymToKeycode(dpy, XK_q), Mod4Mask, root, True, GrabModeAsync, GrabModeAsync);
	XGrabKey(dpy, XKeysymToKeycode(dpy, XK_q), Mod4Mask|ControlMask|ShiftMask, root, True, GrabModeAsync, GrabModeAsync);
	XGrabKey(dpy, XKeysymToKeycode(dpy, XK_b), Mod4Mask, root, True, GrabModeAsync, GrabModeAsync);

	XGrabKey(dpy, XKeysymToKeycode(dpy, XK_k), Mod4Mask, root, True, GrabModeAsync, GrabModeAsync);
	XGrabKey(dpy, XKeysymToKeycode(dpy, XK_l), Mod4Mask, root, True, GrabModeAsync, GrabModeAsync);
	XGrabKey(dpy, XKeysymToKeycode(dpy, XK_k), Mod4Mask|ShiftMask, root, True, GrabModeAsync, GrabModeAsync);
	XGrabKey(dpy, XKeysymToKeycode(dpy, XK_l), Mod4Mask|ShiftMask, root, True, GrabModeAsync, GrabModeAsync);
}

void scan(void) {
	unsigned int num;
	Window d1, d2, *children = NULL;
	XWindowAttributes wa;

	if (XQueryTree(dpy, root, &d1, &d2, &children, &num)) {
		for (int i = 0; i < num; i++) {
			if (!XGetWindowAttributes(dpy, children[i], &wa) || wa.override_redirect) {
				continue;
			}

			if (wa.map_state != IsUnmapped) {
				manage(children[i]);
			}
		}
		if (children) {
			XFree(children);
		}
	}
}

void quit(Bool arg) {
	if (arg) {
		restart = True;
	}
	running = False;
}

void sighup() {
	quit(True);
}

void sigterm() {
	quit(False);
}

void setup() {
	signal(SIGHUP, sighup);
	signal(SIGTERM, sigterm);

	dpy = XOpenDisplay(NULL);
	if (dpy == NULL) {
		panic("Could not open display...");
	}

	root = DefaultRootWindow(dpy);
	root_width = DisplayWidth(dpy, 0);
	root_height = DisplayHeight(dpy, 0);

	XSelectInput(dpy, root, SubstructureRedirectMask|SubstructureNotifyMask|PropertyChangeMask);
	XSync(dpy, False);

	cursor = XCreateFontCursor(dpy, XC_X_cursor);
	XDefineCursor(dpy, root, cursor);
	XSync(dpy, False);

	statusbar = createbar(root_width, BAR_HEIGHT, 0,0);
#ifdef DEBUG
	printf("created bar\n");
#endif
	updatebar(&statusbar);
#ifdef DEBUG
	printf("updated bar\n");
#endif

	grabkeys();

	init();
}

void run() {
	XEvent ev;
	XSync(dpy, False);
	while (running) {
		XNextEvent(dpy, &ev);
		if (handler[ev.type]) {
			handler[ev.type](&ev);
		} else {
#ifdef DEBUG
			printf("No handler for event %d\n", ev.type);
#endif
		}
	}
}

int main(int argc, char *argv[]) {
	setup();
	scan();
	run();
	if (restart) {
		execvp(argv[0], argv);
	}
	XCloseDisplay(dpy);
	return EXIT_SUCCESS;
}
