#include <X11/X.h>
#include <X11/Xft/Xft.h>

typedef struct Client Client;
struct Client {
	char name[64];
	Window wnd;
	Client *prev;
	Client *next;
};

typedef struct Bar Bar;
struct Bar {
	Window wnd;
	char status[128];
	int posx;
	int posy;
	int width;
	int height;
	int padding;
	int border;
	// verbose drawing bs
	XftFont *font;
	Visual *visual;
	Colormap colormap;
	XftDraw *draw;
	XftColor fg_color;
	XftColor bg_color;
	XftColor primary_color;
};
