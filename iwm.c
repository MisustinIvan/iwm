#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xft/Xft.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>
#include <X11/extensions/Xrender.h>
#include <X11/extensions/Xinerama.h>
#include <fontconfig/fontconfig.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
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
void focusmon(Monitor *m);
void unfocus(Client *c);
void manage(Window w);
void unmanage(Window w);
Client *wintoclient(Window wnd);
void updatemon(Monitor *m);
void updatetitle(Client *c);
// bar functions
Bar *createbar(int width, int height, int posx, int posy);
Monitor *bartomon(Bar *b);
void updatestatus(Bar *b);
void updatebar(Bar *b);
void togglebar(Bar *b);
void setbar(Bar *b, Bool arg);
// wm utilities
void spawn(const char *a);
void termclient(Client *c);
void killclient(Client *c);
void init();
void scan();
void initmons();
Monitor *wintomon(Window wnd);
void quit(Bool arg);
void sighup();
void sigterm();
// linked list utils
Client *ripclient(Client *c, Client **head);
void pushclient(Client *c, Client **head);
void insertafter(Client *c, Client *o, Client **head);
void swapnext(Client *c, Client **head);
void swapprev(Client *c, Client **head);

// global variables
// state
Bool running = True;
Bool restart = False;
// Bool bar = True;
// consts
#define BAR_HEIGHT 35
#define BAR_BORDER_WIDTH 2
#define BAR_PADDING 10
// wm stuff
Display *dpy;
Window root;
int root_width;
int root_height;
Monitor *mons = NULL;
Monitor *fmon = NULL;
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
// Bar statusbar;
const char *fontname = "Iosevka Nerd Font Mono:size=15";
static const char *fg_color_const = "#e4e4ef";
static const char *bg_color_const = "#2e3440";
static const char *primary_color_const = "#88c0d0";

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

void termclient(Client *c) {
	Atom a = XInternAtom(dpy, "WM_DELETE_WINDOW", False);

	int n;
	Atom *protocols;
	int exists = False;
	XEvent ev;

	if (XGetWMProtocols(dpy, c->wnd, &protocols, &n)) {
		while (!exists && n--)
			exists = protocols[n] == a;
		XFree(protocols);
	}
	if (exists) {
		ev.type = ClientMessage;
		ev.xclient.window = c->wnd;
		ev.xclient.message_type = XInternAtom(dpy, "WM_PROTOCOLS", False);
		ev.xclient.format = 32;
		ev.xclient.data.l[0] = a;
		ev.xclient.data.l[1] = CurrentTime;
		XSendEvent(dpy, c->wnd, False, NoEventMask, &ev);
	} else {
		killclient(c);
	}
}

void killclient(Client *c) {
	if (c == NULL) return;
	XGrabServer(dpy);
	XSetCloseDownMode(dpy, DestroyAll);
	XKillClient(dpy, c->wnd);
	XSync(dpy, False);
	XUngrabServer(dpy);
}

void init() {
	system("xrandr --output HDMI-1 --above eDP-1");
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

Bar *createbar(int width, int height, int posx, int posy) {
	Bar *b = malloc(sizeof(Bar));
	// create the bar window
	b->wnd = XCreateSimpleWindow(dpy, root, posx, posy, width, height, 0, 0, 0);
	// receive expose events to redraw the bar
	XSelectInput(dpy, b->wnd, ExposureMask);
	// set override redirect so that we dont manage our own window
	XSetWindowAttributes attr;
	attr.override_redirect = True;
	XChangeWindowAttributes(dpy, b->wnd, CWOverrideRedirect, &attr);
	// map the window
	XMapWindow(dpy, b->wnd);
	// load the font
	loadfont(&b->font, fontname);
	// init the drawing context
	b->visual = DefaultVisual(dpy, 0);
	b->colormap = DefaultColormap(dpy, 0);
	XftColorAllocName(dpy, b->visual, b->colormap, fg_color_const, &b->fg_color);
	XftColorAllocName(dpy, b->visual, b->colormap, bg_color_const, &b->bg_color);
	XftColorAllocName(dpy, b->visual, b->colormap, primary_color_const, &b->primary_color);
	b->draw = XftDrawCreate(dpy, b->wnd, b->visual, b->colormap);
	// fill out the remaining fields
	b->width = width;
	b->height = height;
	b->posx = posx;
	b->posy = posy;
	b->border = BAR_BORDER_WIDTH;
	b->padding = BAR_PADDING;

	return b;
}

Monitor *bartomon(Bar *b) {
	return wintomon(b->wnd);
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
	Monitor *bm = bartomon(b);
	if (bm == NULL) return;

	XClearWindow(dpy, b->wnd);
	XftDrawRect(b->draw, &b->bg_color, 0, 0, b->width, b->height);

	updatestatus(b);

	// draw status
	XGlyphInfo status_extents;
	XftTextExtents8(dpy, b->font, (const FcChar8*)b->status, strlen(b->status), &status_extents);

	XftDrawRect(b->draw, &b->primary_color, b->width - status_extents.width - 2*b->border - 2*b->padding, 0, status_extents.width + 2*b->border + 2*b->padding, b->height);
	XftDrawRect(b->draw, &b->bg_color,      b->width - status_extents.width - b->border - 2*b->padding, b->border, status_extents.width + 2*b->padding, b->height - 2*b->border);
	XftDrawString8(b->draw, &b->fg_color, b->font, b->width - status_extents.width - b->border - b->padding, b->height - b->border - 8, (const FcChar8*)b->status, strlen(b->status));
	
	if (bm->clients == NULL) {
		const char *msg = "No clients";
		XftDrawStringUtf8(b->draw, &b->fg_color, b->font, b->border + b->padding, b->height - b->border - 8, (const FcChar8*)msg, strlen(msg));
		XSync(dpy, False);
		return;
	}

	// fill with primary so that integer division doesn't fuck this up
	XftDrawRect(b->draw, &b->primary_color, 0, 0, b->width - status_extents.width - 2*b->border - 2*b->padding, b->height);

	// calculate nclients
	int nclients = 0;
	{
		Client *cc = bm->clients;
		while (cc != NULL) {
			cc = cc->next;
			nclients += 1;
		}
	}

	// calculate character width
	int char_width = 0;
	{
		XGlyphInfo char_extents;
		XftTextExtents8(dpy, b->font, (const FcChar8*)"a", 2, &char_extents);
		char_width = char_extents.width/2;
	}

	int rwidht = b->width - status_extents.width - 2*b->border - 2*b->padding;
	int cwidth = rwidht/nclients;
	int width = 0;
	for (Client *c = bm->clients; c != NULL; c = c->next) {
		XGlyphInfo extents;
		XftTextExtents8(dpy, b->font, (const FcChar8*)c->name, strlen(c->name), &extents);

		// to keep the padding good
		int amount = (cwidth/char_width) - 2;
		if (amount >= strlen(c->name)) {
			amount = strlen(c->name);
		}

		if (c == bm->focused && fmon == bm) {
			XftDrawRect(b->draw, &b->primary_color, width, 0, cwidth, b->height);

			XftDrawString8(b->draw, &b->bg_color, b->font, width + b->border + b->padding, b->height - b->border - 8, (const FcChar8*)c->name, amount);
		} else {
			XftDrawRect(b->draw, &b->primary_color, width, 0, cwidth, b->height);
			XftDrawRect(b->draw, &b->bg_color, width + b->border, b->border, cwidth - 2*b->border, b->height - 2*b->border);

			XftDrawString8(b->draw, &b->fg_color, b->font, width + b->border + b->padding, b->height - b->border - 8, (const FcChar8*)c->name, amount);
		}

		width += cwidth;
	}

	XSync(dpy, False);
}

// updatewm became updatemon, because we have more than one monitor(screen)
void updatemon(Monitor *m) {
	if (m == NULL) return;

	Client *c = m->clients;
	XWindowChanges changes;

	while (c != NULL) {
		if (m->bar) {
			changes.height = m->height - m->statusbar->height;
			changes.y = m->posy + m->statusbar->height;
		} else {
			changes.height = m->height;
			changes.y = m->posy;
		}

		changes.width = m->width;
		changes.x = m->posx;
		changes.stack_mode = Above;
	
		XConfigureWindow(dpy, c->wnd, CWHeight|CWWidth|CWY|CWX|CWStackMode, &changes);

		c = c->next;
	}

	// when moving window to empty client list on another monitor...
	// if (m->focused == NULL && m->clients != NULL) {
	// 	m->focused = m->clients;
	// }

	if (m->focused != NULL) {
		XRaiseWindow(dpy, m->focused->wnd);
	} else {
		m->focused = m->clients;
	}

	if (fmon == m) {
		focus(m->focused);
	}
}

void togglebar(Bar *b) {
	Monitor *m = bartomon(b);
	if (m != NULL) {
		setbar(b, !m->bar);
	}
}

void setbar(Bar *b, Bool arg) {
	Monitor *m = bartomon(b);
	if (m != NULL) {
		m->bar = arg;
		updatemon(m);
	}
}

void configurerequest(XEvent * e) {
	XConfigureRequestEvent *ev = &e->xconfigurerequest;

	Monitor *m = wintomon(ev->window);
	if (m == NULL) {
#ifdef DEBUG
		printf("[CONFIGURE]: Monitor for window: %lu not found, using focused\n", ev->window);
#endif
		if (fmon != NULL) {
			m = fmon;
		} else {
			panic("fmon is NULL???");
		}
	}

	XWindowChanges changes;
	changes.x = m->posx;
	changes.width = m->width;
	if (m->bar) {
		changes.y = m->posy + m->statusbar->height;
		changes.height = m->height - m->statusbar->height;
	} else {
		changes.y = m->posy;
		changes.height = m->height;
	}
	changes.stack_mode = Above;

#ifdef DEBUG
	printf("configurerequest\n");
#endif
	XConfigureWindow(dpy, ev->window, CWX|CWY|CWWidth|CWHeight|CWStackMode, &changes);
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
		if (fmon != NULL) {
				//XDestroyWindow(dpy, fmon->focused->wnd);
			termclient(fmon->focused);
		}
	}

	if (ev->keycode == XKeysymToKeycode(dpy, XK_b) && ev->state == Mod4Mask) {
		if (fmon != NULL) {
			if (fmon->statusbar != NULL) {
				togglebar(fmon->statusbar);
			}
		}
	}

	if (ev->keycode == XKeysymToKeycode(dpy, XK_k) && ev->state == Mod4Mask) {
		if (fmon != NULL) {
			if (fmon->focused != NULL) {
				focus(fmon->focused->prev);
			}
		}
	}

	if (ev->keycode == XKeysymToKeycode(dpy, XK_l) && ev->state == Mod4Mask) {
		if (fmon != NULL) {
			if (fmon->focused != NULL) {
				focus(fmon->focused->next);
			}
		}
	}

	if (ev->keycode == XKeysymToKeycode(dpy, XK_o) && ev->state == Mod4Mask) {
		if (fmon != NULL) {
			if (fmon->next != NULL) {
				focusmon(fmon->next);
			} else if (fmon->prev != NULL) {
				focusmon(fmon->prev);
			}
		}
	}

	if (ev->keycode == XKeysymToKeycode(dpy, XK_period) && ev->state == Mod4Mask) {
		if (fmon != NULL) {
			if (fmon->next != NULL) {
				focusmon(fmon->next);
			}
		}
	}

	if (ev->keycode == XKeysymToKeycode(dpy, XK_comma) && ev->state == Mod4Mask) {
		if (fmon != NULL) {
			if (fmon->prev != NULL) {
				focusmon(fmon->prev);
			}
		}
	}

	if (ev->keycode == XKeysymToKeycode(dpy, XK_period) && ev->state == (Mod4Mask|ShiftMask)) {
		if (fmon != NULL) {
			if (fmon->focused != NULL) {
				Client *prev = fmon->focused->prev;
				Client *next = fmon->focused->next;

				Client *c = ripclient(fmon->focused, &fmon->clients);

				if (prev != NULL) {
					focus(prev);
				} else if (next != NULL) {
					focus(next);
				} else {
					unfocus(c);
				}

				if (fmon->next != NULL) {
					if (fmon->next->focused == NULL) {
						fmon->next->focused = c;
					}
				}

				pushclient(c, &fmon->next->clients);

				updatemon(fmon);
				updatebar(fmon->statusbar);
				updatemon(fmon->next);
				updatebar(fmon->next->statusbar);
			}
		}
	}

	if (ev->keycode == XKeysymToKeycode(dpy, XK_comma) && ev->state == (Mod4Mask|ShiftMask)) {
		if (fmon != NULL) {
			if (fmon->focused != NULL) {
				Client *prev = fmon->focused->prev;
				Client *next = fmon->focused->next;

				Client *c = ripclient(fmon->focused, &fmon->clients);

				if (prev != NULL) {
					focus(prev);
				} else if (next != NULL) {
					focus(next);
				} else {
					unfocus(c);
				}

				if (fmon->prev != NULL) {
					if (fmon->prev->focused == NULL) {
						fmon->prev->focused = c;
					}
				}

				pushclient(c, &fmon->prev->clients);

				updatemon(fmon);
				updatebar(fmon->statusbar);
				updatemon(fmon->prev);
				updatebar(fmon->prev->statusbar);
			}
		}
	}

	if (ev->keycode == XKeysymToKeycode(dpy, XK_k) && ev->state == (Mod4Mask|ShiftMask)) {
		if (fmon != NULL) {
			swapprev(fmon->focused, &fmon->clients);
			updatebar(fmon->statusbar);
		}
	}

	if (ev->keycode == XKeysymToKeycode(dpy, XK_l) && ev->state == (Mod4Mask|ShiftMask)) {
		if (fmon != NULL) {
			swapnext(fmon->focused, &fmon->clients);
			updatebar(fmon->statusbar);
		}
	}
}

void expose(XEvent * e) {
	XExposeEvent *ev = &e->xexpose;
#ifdef DEBUG
	printf("An expose event has been triggered\n");
#endif

	Monitor *m = wintomon(ev->window);
	if (ev->window == m->statusbar->wnd) {
		updatebar(m->statusbar);
	}
}

void propertynotify(XEvent *e) {
	XPropertyEvent *ev = &e->xproperty;

	if (ev->window == root) {
		Monitor *cm = mons;
		while (cm != NULL) {
			updatebar(cm->statusbar);
			cm = cm->next;
		}
	}

	if (ev->atom == XA_WM_NAME) {
		Client *c = wintoclient(ev->window);
		if (c != NULL) {
			updatetitle(c);
			Monitor *m = wintomon(c->wnd);
			if (m != NULL) {
				updatebar(m->statusbar);
			}
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

	Monitor *m = wintomon(c->wnd);
	if (m == NULL) {
		return;
	}

	m->focused = c;
	XSetInputFocus(dpy, c->wnd, RevertToPointerRoot, CurrentTime);
	XRaiseWindow(dpy, c->wnd);
	XSync(dpy, False);
	updatebar(m->statusbar);
}

void unfocus(Client *c) {
	if (c == NULL) {
		return;
	}

	Monitor *m = wintomon(c->wnd);
	if (m == NULL) {
		m = fmon;
		if (m == NULL) {
			return;
		}
	}

	m->focused = NULL;
#ifdef DEBUG
	printf("focusing root\n");
#endif
	XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
	XSync(dpy, False);
	updatebar(m->statusbar);
}

void focusmon(Monitor *m) {
	if (m == NULL) {
		return;
	}

	Client *old_focused = fmon->focused;

	Monitor *prev_monitor = fmon;
	fmon = m;

	if (m->focused != NULL) {
		focus(m->focused);
	} else if (m->clients != NULL) {
		focus(m->clients);
	} else {
		unfocus(old_focused);
	}
	updatebar(prev_monitor->statusbar);
	updatebar(fmon->statusbar);
}

Client *wintoclient(Window wnd) {
	Monitor *m = wintomon(wnd);
	if (m == NULL) {
		return NULL;
	}

	Client *cc = m->clients;
	while (cc != NULL && cc->wnd != wnd) {
		cc = cc->next;
	}
	return cc;
}

// register a window with the window manager
void manage(Window wnd) {
	Client *c = malloc(sizeof(Client));
	c->wnd = wnd;

	Monitor *m = wintomon(wnd);
	if (m == NULL) {
#ifdef DEBUG
		printf("[MANAGE]: monitor for window: %lu not found\n", wnd);
#endif
		if (fmon != NULL) {
			m = fmon;
#ifdef DEBUG
			printf("Using focused monitor\n");
#endif
		} else {
			panic("fmon is NULL??? fuck this");
		}
	}

	XSelectInput(dpy, c->wnd, PropertyChangeMask);
	XSync(dpy, False);

	updatetitle(c);
	pushclient(c, &m->clients);
	focus(c);

#ifdef DEBUG
	printf("Managing %lu:%s\n", c->wnd, c->name);
#endif

	XWindowChanges changes;
	changes.x = m->posx;
	changes.width = m->width;
	if (m->bar) {
		changes.y = m->posy + m->statusbar->height;
		changes.height = m->height - m->statusbar->height;
	} else {
		changes.y = m->posy;
		changes.height = m->height;
	}
	changes.stack_mode = Above;
#ifdef DEBUG
	printf("[MANAGING]\n");
	printf("Changes x: %d\n", changes.x);
	printf("Changes y: %d\n", changes.y);
	printf("Changes width: %d\n", changes.width);
	printf("Changes height: %d\n", changes.height);
#endif

	XConfigureWindow(dpy, c->wnd, CWX|CWY|CWWidth|CWHeight|CWStackMode, &changes);
	updatebar(m->statusbar);
}

void unmanage(Window wnd) {
	Client *c = wintoclient(wnd);
	if (c == NULL) {
		return;
	}

	Monitor *m = wintomon(wnd);
	if (m == NULL) {
#ifdef DEBUG
		printf("[UNMANAGE]: monitor for window: %lu not found\n", wnd);
#endif
		return;
	}

	if (m->focused == c) {
		unfocus(c);
		if (c->prev != NULL) {
			focus(c->prev);
		} else if (c->next != NULL) {
			focus(c->next);
		}
	}

	ripclient(c, &m->clients);

#ifdef DEBUG
	printf("Unmanaging %lu\n", c->wnd);
#endif
	free(c);

#ifdef DEBUG
	printf("updating from unmanage\n");
#endif
	updatebar(m->statusbar);
}

void grabkeys() {
	XGrabKey(dpy, XKeysymToKeycode(dpy, XK_Return), Mod4Mask, root, True, GrabModeAsync, GrabModeAsync);
	XGrabKey(dpy, XKeysymToKeycode(dpy, XK_space), Mod4Mask, root, True, GrabModeAsync, GrabModeAsync);
	XGrabKey(dpy, XKeysymToKeycode(dpy, XK_r), Mod4Mask|ControlMask, root, True, GrabModeAsync, GrabModeAsync);
	XGrabKey(dpy, XKeysymToKeycode(dpy, XK_q), Mod4Mask, root, True, GrabModeAsync, GrabModeAsync);
	XGrabKey(dpy, XKeysymToKeycode(dpy, XK_q), Mod4Mask|ControlMask|ShiftMask, root, True, GrabModeAsync, GrabModeAsync);
	XGrabKey(dpy, XKeysymToKeycode(dpy, XK_b), Mod4Mask, root, True, GrabModeAsync, GrabModeAsync);
	XGrabKey(dpy, XKeysymToKeycode(dpy, XK_period), Mod4Mask, root, True, GrabModeAsync, GrabModeAsync);
	XGrabKey(dpy, XKeysymToKeycode(dpy, XK_comma), Mod4Mask, root, True, GrabModeAsync, GrabModeAsync);
	XGrabKey(dpy, XKeysymToKeycode(dpy, XK_period), Mod4Mask|ShiftMask, root, True, GrabModeAsync, GrabModeAsync);
	XGrabKey(dpy, XKeysymToKeycode(dpy, XK_comma), Mod4Mask|ShiftMask, root, True, GrabModeAsync, GrabModeAsync);
	XGrabKey(dpy, XKeysymToKeycode(dpy, XK_o), Mod4Mask, root, True, GrabModeAsync, GrabModeAsync);

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

void initmons() {
	if (XineramaIsActive(dpy)) {
		int nmons;
		XineramaScreenInfo *info = XineramaQueryScreens(dpy, &nmons);
		if (info == NULL) {
			panic("Could not query Xinerama screens...");
		}

		Monitor *first = NULL;
		Monitor *prev = NULL;

		for (int i = 0; i < nmons; i++) {
			Monitor *mon = malloc(sizeof(Monitor));

			mon->posx = info[i].x_org;
			mon->posy = info[i].y_org;
			mon->width = info[i].width;
			mon->height = info[i].height;
			mon->statusbar = createbar(mon->width, BAR_HEIGHT, mon->posx, mon->posy);
			mon->bar = True;
			mon->clients = NULL;
			mon->focused = NULL;

			if (first == NULL) {
				mon->prev = NULL;
				mon->next = NULL;
				first = mon;
			} else {
				prev->next = mon;
				mon->prev = prev;
				mon->next = NULL;
			}

			updatebar(mon->statusbar);

			prev = mon;
		}
		// dont fucking forget XD
		mons = first;
		fmon = first;

		XFree(info);
#ifdef DEBUG
		printf("created multiple or single mon\n");
#endif
	} else {
		Monitor *mon = malloc(sizeof(Monitor));

		mon->posx = 0;
		mon->posy = 0;
		mon->width = root_width;
		mon->height = root_height;
		mon->statusbar = createbar(mon->width, BAR_HEIGHT, mon->posx, mon->posy);
		mon->bar = True;
		mon->clients = NULL;
		mon->focused = NULL;
		mon->prev = NULL;
		mon->next = NULL;

		mons = mon;
		fmon = mon;

		updatebar(mon->statusbar);

#ifdef DEBUG
		printf("created single mon\n");
#endif
	}
}

Monitor *wintomon(Window w) {
	Monitor *cm = mons;
	while (cm != NULL) {
		if (cm->statusbar->wnd == w) {
			return cm;
		}

		Client *cc = cm->clients;
		while (cc != NULL) {
			if (cc->wnd == w) {
				return cm;
			}
			cc = cc->next;
		}
		cm = cm->next;
	}
	return cm;
}

// unused for now
void free_monitor(Monitor *m) {
	if (m->statusbar != NULL) {
		free(m->statusbar);
	}
	free(m);
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

Client *ripclient(Client *c, Client **head) {
	if (c == NULL || head == NULL || *head == NULL) return NULL;

	Client *left = c->prev;
	Client *right = c->next;

	if (left != NULL) left->next = right;
	if (right != NULL) right->prev = left;
	if (c == *head) *head = right;

	c->next = NULL;
	c->prev = NULL;

	return c;
}

void pushclient(Client *c, Client **head) {
	if (c == NULL || head == NULL) return;

	if (*head == NULL) {
		*head = c;
		c->prev = NULL;
		c->next = NULL;
		return;
	}

	Client *cc = *head;
	while (cc->next != NULL) {
		cc = cc->next;
	}

	cc->next = c;
	c->prev = cc;
	c->next = NULL;
}

void insertafter(Client *c, Client *o, Client **head) {
	if (c == NULL) return;
	if (head == NULL) return;

	if (o == NULL) {
		c->next = (*head);
		if (*head != NULL) (*head)->prev = c;
		*head = c;
		c->prev = NULL;
	} else {
		Client *right = o->next;

		o->next = c;
		c->prev = o;
		c->next = right;
		if (right != NULL) right->prev = c;
	}
}

void swapnext(Client *c, Client **head) {
    if (c == NULL || head == NULL || c->next == NULL || *head == NULL) return;

	Client *o = c->next;
	Client *left = c->prev;
	Client *right = o->next;

	if (left != NULL) left->next = o;
	if (right != NULL) right->prev = c;

	o->prev = left;
	c->next = right;

	o->next = c;
	c->prev = o;

	if (c == *head) *head = o;
}

void swapprev(Client *c, Client **head) {
	if (c == NULL || head == NULL || c->prev == NULL || *head == NULL) return;

	Client *o = c->prev;
	Client *left = o->prev;
	Client *right = c->next;

	if (left != NULL) left->next = c;
	if (right != NULL) right->prev = o;

	o->next = right;
	c->prev = left;

	o->prev = c;
	c->next = o;

	if (o == *head) *head = c;
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

	initmons();

//	// TODO: fix memory bullshit
	grabkeys();
	// prayge
	XSync(dpy, False);
	init();
	// prayge
	XSync(dpy, False);
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
