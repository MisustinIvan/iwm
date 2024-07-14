#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include "util.h"

Display *dpy;
Window root;
Cursor cursor;

typedef struct Client Client;
struct Client {
	Window wnd;
	int x;
	int y;
	int w;
	int h;
	Client *next;
	Client *prev;
};

Client *clients = NULL;
Client *focused = NULL;

int root_w;
int root_h;

void spawn(const char *cmd) {
    if (fork() == 0) {
        if (dpy) {
            close(ConnectionNumber(dpy));
        }
        setsid();
        setenv("DISPLAY", getenv("DISPLAY"), 1);
        execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
        fprintf(stderr, "execl failed: %s\n", cmd);
        exit(EXIT_FAILURE);
    }
}

void get_window_dimensions(Display *dpy, Window wnd, int *w, int *h) {
	XWindowAttributes attrs;
	XGetWindowAttributes(dpy, wnd, &attrs);
	*w = attrs.width;
	*h = attrs.height;
}

void manage(Window w) {
	printf("We make manage %lu's ass\n", w);
	XResizeWindow(dpy, w, root_w, root_h);

	XWindowAttributes attrs;
	XGetWindowAttributes(dpy, w, &attrs);

	// update client list
	Client *c = clients;
	while (c != NULL) {
		if (c->wnd == w) {
			c->x = attrs.x;
			c->y = attrs.y;
			c->w = attrs.width;
			c->h = attrs.height;
			break;
		}
		c = c->next;
	}
}

void handle_create_notify(XEvent e) {
	// add the window to the list of clients
	Client *c = malloc(sizeof(Client));
	c->wnd = e.xcreatewindow.window;
	c->next = clients;
	c->prev = NULL;
	c->x = e.xcreatewindow.x;
	c->y = e.xcreatewindow.y;
	c->w = e.xcreatewindow.width;
	c->h = e.xcreatewindow.height;
	clients = c;

	int len = 0;
	Client *tmp = clients;
	while (tmp != NULL) {
		len++;
		tmp = tmp->next;
	}
	printf("There are %d clients\n", len);

	printf("A window (,,>﹏<,,) announces that it has been created\n");
}

void handle_destroy_notify(XEvent e) {
	printf("A window (,,>﹏<,,) announces that it has been destroyed\n");

	// remove the window from the list of clients
	Client *c = clients;
	while (c != NULL) {
		if (c->wnd == e.xdestroywindow.window) {
			if (c->prev != NULL) {
				c->prev->next = c->next;
			} else {
				clients = c->next;
			}
			if (c->next != NULL) {
				c->next->prev = c->prev;
			}
			free(c);
			break;
		}
		c = c->next;
	}
}

void handle_map_request(XEvent e) {
	printf("A window (,,>﹏<,,) announces that it wants to be mapped\n");
	XMapWindow(dpy, e.xmaprequest.window);
	manage(e.xmaprequest.window);
}

void handle_map_notify(XEvent e) {
	XSetInputFocus(dpy, e.xmap.window, RevertToPointerRoot, CurrentTime);
	focused = clients;
	printf("A window (,,>﹏<,,) announces that it has been mapped\n");
}

void handle_unmap_notify(XEvent e) {
	
	// focus another window
	
	Client *c = clients;
	while (c != NULL) {
		if (c->wnd == e.xunmap.window) {

			if (c->prev != NULL) {
				XSetInputFocus(dpy, c->prev->wnd, RevertToPointerRoot, CurrentTime);
			} else if (c->next != NULL) {
				XSetInputFocus(dpy, c->next->wnd, RevertToPointerRoot, CurrentTime);
			}

			free(c);
			break;
		}
		c = c->next;
	}

	printf("A window (,,>﹏<,,) announces that it has been unmapped\n");
}

void handle_configure_request(XEvent e) {
	printf("A window (,,>﹏<,,) wants to be configured\n");
	XWindowChanges changes;
	// TODO MORE STUFF
	manage(e.xconfigurerequest.window);
}

void handle_configure_notify(XEvent e) {
	printf("A window (,,>﹏<,,) announces that it has been configured\n");
}

int main(void) {
	dpy = XOpenDisplay(NULL);
	if (dpy == NULL) {
		panic("Could not open display...");
	}
	root = DefaultRootWindow(dpy);

	// fetch some info
	get_window_dimensions(dpy, root, &root_w, &root_h);

	// now we register for SubstructureRedirect and SubstructureNotify events
	int err = XSelectInput(dpy, root, SubstructureRedirectMask | SubstructureNotifyMask);
	if (err == BadWindow) {
		panic("Bad window...");
	}
	// sync the changes...
	XSync(dpy, false);

	// mouse and keyboard stuff...
	//
	// create the cursor
	cursor = XCreateFontCursor(dpy, XC_X_cursor);
	// define the cursor
	XDefineCursor(dpy, root, cursor);
	// sync the changes
	XSync(dpy, false);
	//
	// register for mouse events...
	// XGrabButton(dpy, AnyButton, None, root, false, ButtonPressMask, GrabModeAsync, GrabModeAsync, None, cursor);
	// sync the changes
	// XSync(dpy, false);
	//
	// register for key events...
	// XGrabKey(dpy, AnyKey, None, root, false, GrabModeSync, GrabModeAsync);
	// sync the changes
	// XSync(dpy, false);
	//
	KeyCode enter = XKeysymToKeycode(dpy, XStringToKeysym("Return"));
	XGrabKey(dpy, enter, Mod4Mask, root, false, GrabModeAsync, GrabModeAsync);
	KeyCode key_c = XKeysymToKeycode(dpy, XStringToKeysym("c"));
	XGrabKey(dpy, key_c, Mod4Mask, root, false, GrabModeAsync, GrabModeAsync);

	KeyCode key_k = XKeysymToKeycode(dpy, XStringToKeysym("k"));
	XGrabKey(dpy, key_k, Mod4Mask, root, false, GrabModeAsync, GrabModeAsync);
	KeyCode key_l = XKeysymToKeycode(dpy, XStringToKeysym("l"));
	XGrabKey(dpy, key_l, Mod4Mask, root, false, GrabModeAsync, GrabModeAsync);
	// KeyCode key_q = XKeysymToKeycode(dpy, XStringToKeysym("q"));
	// XGrabKey(dpy, key_q, Mod4Mask, root, false, GrabModeAsync, GrabModeAsync);

	// The event we are handling...
	XEvent e;
	// main loop
	for (;;) {
		XNextEvent(dpy, &e);
		switch (e.type) {
			case CreateNotify:
				handle_create_notify(e);
				break;

			case DestroyNotify:
				handle_destroy_notify(e);
				break;

			case MapRequest:
				handle_map_request(e);
				break;

			case MapNotify:
				handle_map_notify(e);
				break;

			case UnmapNotify:
				handle_unmap_notify(e);
				break;

			case ConfigureRequest:
				handle_configure_request(e);
				break;

			case ConfigureNotify:
				handle_configure_notify(e);
				break;

			case ButtonPress:
				printf("Pressed mouse button %d\n", e.xbutton.button);
				XAllowEvents(dpy, ReplayPointer, CurrentTime);
				break;

			case KeyPress:
				printf("Pressed key %d\n", e.xkey.keycode);

				if (e.xkey.keycode == key_k && (e.xkey.state & Mod4Mask)) {
					focused = focused->prev;
					if (focused == NULL) {
						focused = clients;
					}

					XSetInputFocus(dpy, focused->wnd, RevertToPointerRoot, CurrentTime);
				}

				if (e.xkey.keycode == key_l && (e.xkey.state & Mod4Mask)) {
					focused = focused->next;
					if (focused == NULL) {
						focused = clients;
					}

					XSetInputFocus(dpy, focused->wnd, RevertToPointerRoot, CurrentTime);
				}

				if (e.xkey.keycode == enter && (e.xkey.state & Mod4Mask)) {
					printf("Starting alacritty...\n");
					spawn("alacritty");
				}

				if (e.xkey.keycode == key_c && (e.xkey.state & Mod4Mask)) {
					printf("Exitting...\n");
					XCloseDisplay(dpy);
					exit(EXIT_SUCCESS);
				}

				XAllowEvents(dpy, ReplayKeyboard, CurrentTime);
				break;

			case KeyRelease:
				printf("Released key %d\n", e.xkey.keycode);
				XAllowEvents(dpy, ReplayKeyboard, CurrentTime);
				break;

			default:
				printf("Unhandled event... %d\n", e.type);
				break;
		}
		// sync the changes...
		XSync(dpy, false);
	}

	XCloseDisplay(dpy);
	return EXIT_SUCCESS;
}
