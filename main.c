#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <time.h>
#include <X11/Xutil.h>
#include <X11/Xft/Xft.h>

#pragma clang diagnostic push
#pragma ide diagnostic ignored "NullDereferences"

enum {
    SchemeNorm, SchemeSel, SchemeOut, SchemeLast
}; /* color schemes */

static const char *colors[SchemeLast][2] = {
        /*     fg         bg       */
        [SchemeNorm] = {"#a6c2e8", "#090408"},
        [SchemeSel] = {"#a6c2e8", "#99210A"},
        [SchemeOut] = {"#a6c2e8", "#3147C7"},
};


typedef struct {
    Cursor cursor;
} Cur;

struct Fnt {
    Display *dpy;
    unsigned int h;
    XftFont *xfont;
    FcPattern *pattern;
    struct Fnt *next;
};

enum {
    ColFg, ColBg
}; /* Clr scheme index */
typedef XftColor Clr;

static const char *use_fonts[] = {
        "JetBrainsMono:size=14"
};

int32_t bh, mw, mh;

Clr *scheme[SchemeLast];

int32_t lrpad;

Atom clip, utf8;

XIC xic;

Window win;

struct Drw {
    unsigned int w, h;
    Display *dpy;
    int screen;
    Window root;
    Drawable drawable;
    GC gc;
    Clr *scheme;
    struct Fnt *fonts;
};

struct Drw *drw;

void xfont_free(struct Fnt *font) {
    if (!font) {
        return;
    }
    if (font->pattern) {
        FcPatternDestroy(font->pattern);
    }
    XftFontClose(font->dpy, font->xfont);
    free(font);
}

void drw_fontset_free(struct Fnt *font) {
    if (font) {
        drw_fontset_free(font->next);
        xfont_free(font);
    }
}

void drw_free() {
    if (drw->drawable) {
        XFreePixmap(drw->dpy, drw->drawable);
    }
    if (drw->gc) {
        XFreeGC(drw->dpy, drw->gc);
    }
    if (drw->fonts) {
        drw_fontset_free(drw->fonts);
    }
    if (drw) {
        free(drw);
    }
}

void cleanup() {
    size_t i;

    if (win) {
        XDestroyWindow(drw->dpy, win);
    }

    if (drw) {
        XUngrabKey(drw->dpy, AnyKey, AnyModifier, drw->root);
        XSync(drw->dpy, False);
        XCloseDisplay(drw->dpy);
    }
    for (i = 0; i < SchemeLast; ++i) {
        free(scheme[i]);
    }
    drw_free();
}

void die(uint32_t code) {
    cleanup();
    exit(code);
}

void *ecalloc(size_t nmemb, size_t size) {
    void *p = calloc(nmemb, size);
    if (p == NULL) {
        fputs("Could not allocate memory!\n", stderr);
        die(1);
    }
    return p;
}

struct Fnt *xfont_create(const char *fontname, FcPattern *fontpattern) {
    struct Fnt *font;
    XftFont *xfont = NULL;
    FcPattern *pattern = NULL;

    if (fontname) {
        if (!(xfont = XftFontOpenName(drw->dpy, drw->screen, fontname))) {
            fprintf(stderr, "Could not load font from name '%s'!\n", fontname);
            return NULL;
        }
        if (!(pattern = FcNameParse((FcChar8 *) fontname))) {
            fprintf(stderr, "Could not parse font name to pattern: '%s'!\n", fontname);
            XftFontClose(drw->dpy, xfont);
            return NULL;
        }
    } else if (fontpattern) {
        if (!(xfont = XftFontOpenPattern(drw->dpy, fontpattern))) {
            fprintf(stderr, "Could not load font from pattern: '%s'!\n", fontpattern);
            return NULL;
        }
    } else {
        fputs("No font specified!\n", stderr);
        die(1);
    }

    font = ecalloc(1, sizeof(struct Fnt));
    font->xfont = xfont;
    font->pattern = pattern;
    font->h = xfont->ascent + xfont->descent;
    font->dpy = drw->dpy;

    return font;
}

struct Fnt *drw_fontset_create(const char *fonts[], size_t fontcount) {
    struct Fnt *cur, *ret = NULL;
    size_t i;

    if (!drw || !fonts) {
        return NULL;
    }

    for (i = 1; i <= fontcount; ++i) {
        if ((cur = xfont_create(fonts[fontcount - i], NULL))) {
            cur->next = ret;
            ret = cur;
        }
    }
    return (drw->fonts = ret);
}

struct Drw *drw_create(Display *dpy, int screen, Window root, unsigned int w, unsigned int h) {
    struct Drw *t_drw = (struct Drw *) ecalloc(1, sizeof(struct Drw));

    t_drw->dpy = dpy;
    t_drw->screen = screen;
    t_drw->root = root;
    t_drw->h = h;
    t_drw->w = w;
    t_drw->drawable = XCreatePixmap(dpy, root, w, h, DefaultDepth(dpy, screen));
    t_drw->gc = XCreateGC(dpy, root, 0, NULL);

    return t_drw;
}

void keyboard_grab(Display *dpy) {
    struct timespec ts = {.tv_sec = 0, .tv_nsec = 1000000};
    uint32_t i;
    for (i = 0; i < 1000; ++i) {
        if (XGrabKeyboard(dpy, DefaultRootWindow(dpy), True, GrabModeAsync, GrabModeAsync, CurrentTime) ==
            GrabSuccess) {
            return;
        }
        nanosleep(&ts, NULL);
    }
    fputs("Could not grab the keyboard!\n", stderr);
    die(1);
}

void drw_clr_create(Clr *dest, const char *clrname) {
    if (!drw || !dest || !clrname) {
        return;
    }

    if (!XftColorAllocName(drw->dpy,
                           DefaultVisual(drw->dpy, drw->screen),
                           DefaultColormap(drw->dpy, drw->screen),
                           clrname, dest)) {
        fputs("Could not allocate colour!\n", stderr);
        die(1);
    }
}

Clr *drw_scm_create(const char *clrnames[], size_t clrcount) {
    size_t i;
    Clr *ret;

    if (!drw || !clrnames || clrcount < 2 || !(ret = (Clr *) ecalloc(clrcount, sizeof(XftColor)))) {
        return NULL;
    }

    for (i = 0; i < clrcount; ++i) {
        drw_clr_create(&ret[i], clrnames[i]);
    }

    return ret;
}

void drw_resize(uint32_t w, uint32_t h) {
    if (!drw) {
        return;
    }

    drw->w = w;
    drw->h = h;
    if (drw->drawable) {
        XFreePixmap(drw->dpy, drw->drawable);
    }

    drw->drawable = XCreatePixmap(drw->dpy, drw->root, w, h, DefaultDepth(drw->dpy, drw->screen));
}

void drw_setscheme(Clr *scm) {
    if (drw) {
        drw->scheme = scm;
    }
}

void drw_rect(int32_t x, int32_t y, uint32_t w, uint32_t h, int32_t filled, int32_t invert) {
    if (!drw || !drw->scheme) {
        return;
    }
    XSetForeground(drw->dpy, drw->gc, invert ? drw->scheme[ColBg].pixel : drw->scheme[ColFg].pixel);
    if (filled) {
        XFillRectangle(drw->dpy, drw->drawable, drw->gc, x, y, w, h);
    } else {
        XDrawRectangle(drw->dpy, drw->drawable, drw->gc, x, y, w - 1, h - 1);
    }
}

void draw_field() {
    uint32_t curpos;
    struct item *item;
    int32_t x = 0, y = 0, w;

    drw_setscheme(scheme[SchemeNorm]);
    drw_rect(0, 0, mw, mh, 1, 1);
}

void setup() {
    int32_t x, y;
    XSetWindowAttributes swa;
    XIM xim;
    XWindowAttributes wa;
    XClassHint ch = {"r2k", "r2k"};

    {
        uint32_t i;
        for (i = 0; i < SchemeLast; ++i) {
            scheme[i] = drw_scm_create(colors[i], 2);
        }
    }

    clip = XInternAtom(drw->dpy, "CLIPBOARD", False);
    utf8 = XInternAtom(drw->dpy, "UTF8_STRING", False);

    bh = drw->fonts->h + 2;
    mh = 2 * bh;

    if (!XGetWindowAttributes(drw->dpy, drw->root, &wa)) {
        fprintf(stdout, "Could not get embedding window attributes: 0x%lx", drw->root);
        die(1);
    }

    x = y = 0;
    mw = 300;

    swa.override_redirect = True;
    swa.background_pixel = scheme[SchemeNorm][ColBg].pixel;
    swa.event_mask = ExposureMask | KeyPressMask | VisibilityChangeMask;
    win = XCreateWindow(drw->dpy, drw->root, x, y, mw, mh, 0, CopyFromParent, CopyFromParent, CopyFromParent,
                        CWOverrideRedirect | CWBackPixel | CWEventMask, &swa);
    XSetClassHint(drw->dpy, win, &ch);

    if ((xim = XOpenIM(drw->dpy, NULL, NULL, NULL)) == NULL) {
        fputs("XOpenIM failed: Could not open input device!\n", stderr);
        die(1);
    }

    xic = XCreateIC(xim, XNInputStyle, XIMPreeditNothing | XIMStatusNothing, XNClientWindow, win, XNFocusWindow, win,
                    NULL);

    XMapRaised(drw->dpy, win);

    drw_resize(mw, mh);
    draw_field();
}

void drw_map(Window window, int x, int y, uint32_t w, uint32_t h) {
    if (!drw) {
        return;
    }

    XCopyArea(drw->dpy, drw->drawable, window, drw->gc, x, y, w, h, x, y);
    XSync(drw->dpy, False);
}

void grabfocus() {
    //TODO: Handle focus grabs
}

void keypress(XKeyEvent *ev) {
    //TODO: Handle Keypresses
}

void paste() {
    //TODO: Make paste
}

void run() {
    XEvent ev;
    while (!XNextEvent(drw->dpy, &ev)) {
        if (XFilterEvent(&ev, win)) {
            continue;
        }
        switch (ev.type) {
            case DestroyNotify:
                if (ev.xdestroywindow.window != win) {
                    break;
                }
                die(1);
                break;
            case Expose:
                if (ev.xexpose.count == 0) {
                    drw_map(win, 0, 0, mw, mh);
                }
                break;
            case FocusIn:
                if (ev.xfocus.window != win) {
                    grabfocus();
                }
                break;
            case KeyPress:
                keypress(&ev.xkey);
                break;
            case SelectionNotify:
                if (ev.xselection.property == utf8) {
                    paste();
                }
                break;
            case VisibilityNotify:
                if (ev.xvisibility.state != VisibilityUnobscured) {
                    XRaiseWindow(drw->dpy, win);
                }
                break;
            default:
                break;
        }
    }
}

int main(int argc, char **argv) {

    int32_t screen;
    Display *dpy;
    Window root;
    XWindowAttributes wa;

    if (!(dpy = XOpenDisplay(NULL))) {
        fputs("Could not open the display!\n", stderr);
        die(1);
    }

    screen = DefaultScreen(dpy);
    root = RootWindow(dpy, screen);
    if (!XGetWindowAttributes(dpy, root, &wa)) {
        fputs("Could not get root window attributes!\n", stderr);
        die(1);
    }
    drw = drw_create(dpy, screen, root, wa.width, wa.height);
    if (!drw_fontset_create(use_fonts, sizeof(use_fonts) / sizeof(use_fonts[0]))) {
        fputs("Fonts could not be loaded", stderr);
        die(1);
    }
    lrpad = drw->fonts->h;

    keyboard_grab(drw->dpy);

    if (!XGetWindowAttributes(dpy, root, &wa)) {
        fprintf(stdout, "Could not get embedding window attributes: 0x%lx", root);
        die(1);
    }

    setup();
    run();

    return 0;
}

#pragma clang diagnostic pop