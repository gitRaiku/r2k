#include <stdio.h>
#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xft/Xft.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>
#include <locale.h>

#define R2K_LOG_NAME "R2K"
#include "log.h"

#include "config.h"
#include "type.h"

#define UTF_INVALID 0xFFFD
#define UTF_SIZ 4

const unsigned char utfbyte[UTF_SIZ + 1] = {0x80, 0, 0xC0, 0xE0, 0xF0};
const unsigned char utfmask[UTF_SIZ + 1] = {0xC0, 0x80, 0xE0, 0xF0, 0xF8};
const int64_t utfmin[UTF_SIZ + 1] = {0, 0, 0x80, 0x800, 0x10000};
const int64_t utfmax[UTF_SIZ + 1] = {0x10FFFF, 0x7F, 0x7FF, 0xFFFF, 0x10FFFF};

char text[2048] = {0};
size_t cursor = 0;

uint8_t running;

int32_t client_sock;

char *results[1024];
uint32_t res_count = 0;
uint32_t res_selected = 0;

struct item {
  char *text;
  struct item *left, *right;
  int32_t out;
};

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

int32_t bh, mw, mh;

Clr *scheme[SchemeLast];

int32_t lrpad;

Atom clip, utf8;

XIC xic;

Window root, parentwin, win;

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
  XFreePixmap(drw->dpy, drw->drawable);
  XFreeGC(drw->dpy, drw->gc);
  drw_fontset_free(drw->fonts);
  free(drw);
}

void cleanup() {
  size_t i;

  XUngrabKey(drw->dpy, AnyKey, AnyModifier, drw->root);

  for (i = 0; i < SchemeLast; ++i) {
    free(scheme[i]);
  }

  Display *dpy = drw->dpy;

  drw_free();

  XSync(dpy, False);
  XCloseDisplay(dpy);

}

void die(uint32_t code) {
  cleanup();
  exit(code);
}

void *ecalloc(size_t nmemb, size_t size) {
  void *p = calloc(nmemb, size);
  if (p == NULL) {
    log_string(10, "Could not allocate memory!\n", stderr);
    die(1);
  }
  return p;
}

void hexprint(char *buf, uint32_t l) {
  int32_t i;
  for(i = 0; i < l; ++i) {
    fprintf(stdout, "0x%x ", (uint8_t)buf[i]);
  }
  fputc('\n', stdout);
}

void str_to_dict(char *str, char *buf) {
  uint32_t strl;
  int32_t rc;
  char sendBuf[1024] = {0};
  strl = strlen(str);
  sendBuf[0] = 0b11;
  sendBuf[1] = strl & 0xFF00;
  sendBuf[2] = strl & 0x00FF;
  memcpy(sendBuf + 3, str, strl);
  rc = send(client_sock, sendBuf, strl+3, 0);
  if (rc == -1) {
    log_format(8, stderr, "Could not query the server! [%m]\n");
    buf[1] = buf[2] = 0;
    return;
  }

  rc = recv(client_sock, buf, 2048 * sizeof(*buf), 0);
  if (rc == -1) {
    log_format(8, stderr, "Could not recieve a response from the server! [%m]\n");
    buf[1] = buf[2] = 0;
    return;
  }
}

void update_results() {
  int32_t i;
  for(i = 0; i < res_count; ++i) {
    free(results[i]);
  }

  char buf[2048];
  str_to_dict(text, buf);
  
  uint32_t strl = (uint8_t)buf[1] << 8u | (uint8_t)buf[2];

  char *saveptr = buf + 3;
  char *token;
  uint32_t tl;

  log_format(0, stdout, "strl: %u\n", strl);

  uint32_t cr = 0;
  while ((token = strtok_r(saveptr, "\n", &saveptr))) {
    log_format(0, stdout, "t: %u b: %u d: %u\n", token, buf, token - buf - 3);
    if (token - buf - 3 >= strl) {
      break;
    }
    tl = strlen(token);
    results[cr] = malloc(sizeof(**results) * (tl + 1));
    strcpy(results[cr], token);
    results[cr][tl] = '\0';
    ++cr;
  }
  res_count = cr;
  res_selected = 0;
}

struct Fnt *xfont_create(const char *fontname, FcPattern *fontpattern) {
  struct Fnt *font;
  XftFont *xfont = NULL;
  FcPattern *pattern = NULL;

  if (fontname) {
    if (!(xfont = XftFontOpenName(drw->dpy, drw->screen, fontname))) {
      log_format(5, stderr, "Could not load font from name '%s'!\n", fontname);
      return NULL;
    }
    if (!(pattern = FcNameParse((FcChar8 *) fontname))) {
      log_format(5, stderr, "Could not parse font name to pattern: '%s'!\n", fontname);
      XftFontClose(drw->dpy, xfont);
      return NULL;
    }
  } else if (fontpattern) {
    if (!(xfont = XftFontOpenPattern(drw->dpy, fontpattern))) {
      log_string(5, "Could not load font from pattern!\n", stderr);
      return NULL;
    }
  } else {
    log_string(10, "No font specified!\n", stderr);
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

struct Drw *drw_create(Display *dpy, int screen, unsigned int w, unsigned int h) {
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
  log_string(10, "Could not grab the keyboard!\n", stderr);
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
    log_string(10, "Could not allocate colour!\n", stderr);
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

void drw_rect(int32_t x, int32_t y, uint32_t w, uint32_t h, int32_t filled, uint8_t invert) {
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

int32_t min(int32_t o1, int32_t o2) {
  return o1 < o2 ? o1 : o2;
}

int32_t max(int32_t o1, int32_t o2) {
  return o1 > o2 ? o1 : o2;
}

int64_t utf8decodebyte(const char c, size_t *i) {
  for (*i = 0; *i < (UTF_SIZ + 1); ++(*i)) {
    if (((unsigned char) c & utfmask[*i]) == utfbyte[*i]) {
      return (unsigned char) c & ~utfmask[*i];
    }
  }
  return 0;
}

size_t utf8validate(int64_t *u, size_t i) {
  if (!(utfmin[i] <= *u && *u <= utfmax[i]) || (0xD800 <= *u && *u <= 0xDFFF)) {
    *u = UTF_INVALID;
  }
  for (i = 1; *u > utfmax[i]; ++i);
  return i;
}

size_t utf8decode(const char *c, int64_t *u, size_t clen) {
  size_t i, j, len, type;
  int64_t udecoded;

  *u = UTF_INVALID;
  if (!clen) {
    return 0;
  }
  udecoded = utf8decodebyte(c[0], &len);
  if (!(1 <= len && len <= UTF_SIZ)) {
    return 1;
  }
  for (i = 1, j = 1; i < clen && j < len; ++i, ++j) {
    udecoded = (udecoded << 6) | utf8decodebyte(c[i], &type);
    if (type) {
      return j;
    }
  }
  if (j < len) {
    return 0;
  }
  *u = udecoded;
  utf8validate(u, len);
  return len;
}

void drw_font_getexts(struct Fnt *font, const char *txt, uint32_t len, uint32_t *w, uint32_t *h) {
  XGlyphInfo ext;

  if (!font || !txt) {
    return;
  }

  XftTextExtentsUtf8(font->dpy, font->xfont, (XftChar8 *) txt, len, &ext);
  if (w) {
    *w = ext.xOff;
  }
  if (h) {
    *h = font->h;
  }
}

int32_t drw_text(int32_t x, int32_t y, uint32_t w, uint32_t h, uint32_t lpad, const char *txt, uint8_t invert) {
  char buf[1024];
  int32_t ty;
  uint32_t ew = 0;
  XftDraw *d = NULL;
  struct Fnt *usedfont, *curfont, *nextfont;
  size_t i, len;
  int32_t utf8strlen, utf8charlen, render = x || y || w || h;
  int64_t utf8codepoint = 0;
  const char *utf8str;
  FcCharSet *fccharset;
  FcPattern *fcpattern;
  FcPattern *match;
  XftResult result;
  int32_t charexists = 0;

  if (!drw || (render && !drw->scheme) || !txt || !drw->fonts) {
    return 0;
  }
  if (!render) {
    w = ~w;
  } else {
    XSetForeground(drw->dpy, drw->gc, drw->scheme[invert ? ColFg : ColBg].pixel);
    XFillRectangle(drw->dpy, drw->drawable, drw->gc, x, y, w, h);
    d = XftDrawCreate(drw->dpy, drw->drawable, DefaultVisual(drw->dpy, drw->screen),
              DefaultColormap(drw->dpy, drw->screen));
    x += lpad;
    w -= lpad;
  }

  usedfont = drw->fonts;
  for (;;) {
    utf8strlen = 0;
    utf8str = txt;
    nextfont = NULL;
    while (*txt) {
      utf8charlen = utf8decode(txt, &utf8codepoint, UTF_SIZ);
      for (curfont = drw->fonts; curfont; curfont = curfont->next) {
        charexists = charexists || XftCharExists(drw->dpy, curfont->xfont, utf8codepoint);
        if (charexists) {
          if (curfont == usedfont) {
            utf8strlen += utf8charlen;
            txt += utf8charlen;
          } else {
            nextfont = curfont;
          }
          break;
        }
      }

      if (!charexists || nextfont) {
        break;
      } else {
        charexists = 0;
      }
    }

    if (utf8strlen) {
      drw_font_getexts(usedfont, utf8str, utf8strlen, &ew, NULL);

      for (len = min(utf8strlen, sizeof(buf) - 1); len && ew > w; --len) {
        drw_font_getexts(usedfont, utf8str, len, &ew, NULL);
      }

      if (len) {
        memcpy(buf, utf8str, len);
        buf[len] = '\0';
        if (len < utf8strlen) {
          for (i = len; i && i > len - 3; buf[--i] = '.'); /* NOP */
        }

        if (render) {
          ty = y + (h - usedfont->h) / 2 + usedfont->xfont->ascent;
          XftDrawStringUtf8(d, &drw->scheme[invert ? ColBg : ColFg], usedfont->xfont,
                    x, ty, (XftChar8 *) buf, len);
        }
        x += ew;
        w -= ew;
      }
    }

    if (!*txt) {
      break;
    } else if (nextfont) {
      charexists = 0;
      usedfont = nextfont;
    } else {
      charexists = 1;
      fccharset = FcCharSetCreate();
      FcCharSetAddChar(fccharset, utf8codepoint);

      if (!drw->fonts->pattern) {
        log_string(10, "The first font in the cache must be loaded from a font string!", stderr);
        die(1);
      }

      fcpattern = FcPatternDuplicate(drw->fonts->pattern);
      FcPatternAddCharSet(fcpattern, FC_CHARSET, fccharset);
      FcPatternAddBool(fcpattern, FC_SCALABLE, FcTrue);
      FcPatternAddBool(fcpattern, FC_COLOR, FcFalse);

      FcConfigSubstitute(NULL, fcpattern, FcMatchPattern);
      FcDefaultSubstitute(fcpattern);
      match = XftFontMatch(drw->dpy, drw->screen, fcpattern, &result);

      FcCharSetDestroy(fccharset);
      FcPatternDestroy(fcpattern);

      if (match) {
        usedfont = xfont_create(NULL, match);
        if (usedfont && XftCharExists(drw->dpy, usedfont->xfont, utf8codepoint)) {
          for (curfont = drw->fonts; curfont->next; curfont = curfont->next); /* NOP */
          curfont->next = usedfont;
        } else {
          xfont_free(usedfont);
          usedfont = drw->fonts;
        }
      }
    }
  }
  if (d) {
    XftDrawDestroy(d);
  }

  return x + (render ? w : 0);
}

void drw_map(int x, int y, uint32_t w, uint32_t h) {
  if (!drw) {
    return;
  }

  XCopyArea(drw->dpy, drw->drawable, win, drw->gc, x, y, w, h, x, y);
  XSync(drw->dpy, False);
}

uint32_t drw_fontset_getwidth(const char *txt) {
  if (!drw || !drw->fonts || !txt) {
    return 0;
  }
  return drw_text(0, 0, 0, 0, 0, txt, 0);
}

uint32_t textw(const char *x) {
  return drw_fontset_getwidth(x) + lrpad;
}

/// TODO: Display the second line containing the results
void drawmenu() {
  uint32_t curpos;
  int32_t x = 0, y = 0, w;

  drw_setscheme(scheme[SchemeNorm]);
  drw_rect(0, 0, mw, mh, 1, 1);

  w = mw - x;
  drw_setscheme(scheme[SchemeNorm]);
  drw_text(x, 0, w, bh, lrpad / 2, text, 0);

  curpos = textw(text) - textw(&text[cursor]);
  if ((curpos += lrpad / 2 - 1) < w) {
    drw_setscheme(scheme[SchemeNorm]);
    drw_rect(x + curpos, 2, 2, bh - 4, 1, 0);
  }

  {
    uint32_t i, cur_pos = x;
    for (i = 0; i < res_count; ++i) {
      if (res_selected == i) {
        drw_setscheme(scheme[SchemeSel]);
      } else {
        drw_setscheme(scheme[SchemeNorm]);
      }
      drw_text(cur_pos, y + bh, w, bh, lrpad / 2, results[i], 0);
      cur_pos += textw(results[i]);
    }
  }


  drw_map(0, 0, mw, mh);
}

void grabfocus() {
  struct timespec ts = {.tv_sec = 0, .tv_nsec = 10000000};
  Window focuswin;
  int32_t i, revertwin;
  for (i = 0; i < 100; ++i) {
    XGetInputFocus(drw->dpy, &focuswin, &revertwin);
    if (focuswin == win) {
      return;
    }
    XSetInputFocus(drw->dpy, win, RevertToParent, CurrentTime);
    nanosleep(&ts, NULL);
  }
  log_string(10, "Cannot grab focus!\n", stderr);
}

void setup() {
  int32_t x, y, i;
  uint32_t du;
  XSetWindowAttributes swa;
  XIM xim;
  Window w, dw, *dws;
  XWindowAttributes wa;
  XClassHint ch = {"r2k", "r2k"};

  for (i = 0; i < SchemeLast; ++i) {
    scheme[i] = drw_scm_create(colors[i], 2);
  }

  clip = XInternAtom(drw->dpy, "CLIPBOARD", False);
  utf8 = XInternAtom(drw->dpy, "UTF8_STRING", False);

  bh = drw->fonts->h + 2;
  mh = 2 * bh + 2;

  if (!XGetWindowAttributes(drw->dpy, drw->root, &wa)) {
    log_format(10, stderr, "Could not get embedding window attributes: 0x%lx", drw->root);
    die(1);
  }

  x = 0;
  y = 0;
  mw = wa.width;

  swa.override_redirect = True;
  swa.background_pixel = scheme[SchemeNorm][ColBg].pixel;
  swa.event_mask = ExposureMask | KeyPressMask | VisibilityChangeMask;
  win = XCreateWindow(drw->dpy, drw->root, x, y, mw, mh, 0, CopyFromParent, CopyFromParent, CopyFromParent,
            CWOverrideRedirect | CWBackPixel | CWEventMask, &swa);
  XSetClassHint(drw->dpy, win, &ch);

  if ((xim = XOpenIM(drw->dpy, NULL, NULL, NULL)) == NULL) {
    log_string(10, "XOpenIM failed: Could not open input device!\n", stderr);
    die(1);
  }

  xic = XCreateIC(xim, XNInputStyle, XIMPreeditNothing | XIMStatusNothing, XNClientWindow, win, XNFocusWindow, win,
          NULL);

  XMapRaised(drw->dpy, win);

  if (parentwin != root) {
    XSelectInput(drw->dpy, parentwin, FocusChangeMask | SubstructureNotifyMask);
    if (XQueryTree(drw->dpy, parentwin, &dw, &w, &dws, &du) && dws) {
      for (i = 0; i < du && dws[i] != win; ++i) {
        XSelectInput(drw->dpy, dws[i], FocusChangeMask);
        XFree(dws);
      }
    }
    grabfocus();
  }

  drw_resize(mw, mh);
  drawmenu();
}

size_t nextrune(int32_t inc) {
  ssize_t n;

  /* Return location of next utf8 rune in the given direction (+1 or -1) */
  for (n = cursor + inc; n + inc >= 0 && (text[n] & 0xc0) == 0x80; n += inc);
  return n;
}

void __inline__ print_buf() {
  log_format(0, stdout, "Current buffer: \"%s\", cursor = %lu\n", text, cursor);
}

void insert(const char *str, ssize_t n) {
  log_format(0, stdout, "Inserting: \"%s\", n = %lu\n", str, n);
  if (strlen(text) + n > sizeof(text) - 1) {
    return;
  }
  memmove(&text[cursor + n], &text[cursor], sizeof(text) - cursor - max(n, 0));
  if (n > 0) {
    memcpy(&text[cursor], str, n);
  }
  cursor += n;
  update_results();
  print_buf();
}

void type_selection() { /// TODO: Fuck you
  type_str(results[res_selected]);
}

uint32_t mod(int32_t val, int32_t m) {
  return ((val % m) + m) % m;
}

uint8_t keypress(XKeyEvent *ev) {
  char buf[32];
  int32_t len;
  KeySym ksym;
  Status status;
  log_format(0, stdout, "Pressed: %u\n", ev->keycode);
  len = XmbLookupString(xic, ev, buf, sizeof(buf), &ksym, &status);
  // uint8_t shift = (ev->state & ShiftMask) > 0; 
  if (ev->state & ControlMask) {
    switch (ksym) {
      case XK_C: /* Passthrough */
      case XK_c:
        log_string(0, "Quit signal recieved, dying...\n", stdout);
        die(0);
        break;
      case XK_i:
        ksym = XK_Home;
        break;
      case XK_a:
        ksym = XK_End;
        break;
      case XK_d:
        text[cursor] = '\0';
        break;
      case XK_u:
        insert(NULL, 0 - cursor);
        break;
      default:
        break;
    }
  }

  switch (ksym) {
    case XK_Delete:
      if (text[cursor] == '\0') {
        return 0;
      }
      cursor = nextrune(+1);
      /* Fallthrough */
    case XK_BackSpace:
      if (cursor == 0) {
        return 0;
      }
      insert(NULL, nextrune(-1) - cursor);
      break;
    case XK_End:
      if (text[cursor] != '\0') {
        cursor = strlen(text);
      }
      break;
    case XK_Home:
      cursor = 0;
      break;
    case XK_Left:
      if (cursor) {
        cursor = nextrune(-1);
      }
      break;
    case XK_Right:
      if (text[cursor] != '\0') {
        cursor = nextrune(+1);
      }
      break;
    case XK_space:
    case XK_Down:
      res_selected = mod(res_selected + 1, res_count);
      break;
    case XK_Tab:
    case XK_Up:
      res_selected = mod(res_selected - 1, res_count);
      break;
    case XK_Return:
      type_selection();
      return 1;
      break;
    default:
      if (!iscntrl(*buf)) {
        insert(buf, len);
      }
      break;
  }

  drawmenu();
  return 0;
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
          drw_map(0, 0, mw, mh);
        }
        break;
      case FocusIn:
        if (ev.xfocus.window != win) {
          grabfocus();
        }
        break;
      case KeyPress:
        if (keypress(&ev.xkey) == 1) {
          return;
        }
        break;
//      case SelectionNotify:
//        if (ev.xselection.property == utf8) {
//          paste();
//        }
//        break;
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

void setup_dummy_results() {
  res_count = 2;
  char kamisama[13] = {0xe3, 0x81, 0x8b, 0xe3, 0x81, 0xbf, 0xe3, 0x81, 0x95, 0xe3, 0x81, 0xbe, 0x00};
  char Raiku[6] = {0x52, 0x61, 0x69, 0x6b, 0x75, 0x00};
  /*results = calloc(2, sizeof(results));*/
  results[0] = calloc(13, sizeof(sizeof(results[0])));
  strcpy(results[0], kamisama);
  results[1] = calloc(6, sizeof(sizeof(results[0])));
  strcpy(results[1], Raiku);
}

/// TODO: Find the actual selected window
Window focused_window(Display *dpy) {
//  Window focused;
//  int32_t revert_to;
//  XGetInputFocus(dpy, &focused, &revert_to);
//  return focused;
  return root;
}

void setup_server_connection() {
  int32_t rc;
  socklen_t len;
  struct sockaddr_un ssockaddr = {0}; 
  struct sockaddr_un csockaddr = {0};

  client_sock = socket(AF_UNIX, SOCK_STREAM, 0); 

  csockaddr.sun_family = AF_UNIX;
  strcpy(csockaddr.sun_path, "/tmp/r2k.client");
  len = sizeof(csockaddr);

  unlink("/tmp/r2k.client");
  rc = bind(client_sock, (struct sockaddr *) &csockaddr, len);
  if (rc == -1) {
    log_format(10, stderr, "Could not bind the socket! [%m]\n");
    die(1);
  }

  ssockaddr.sun_family = AF_UNIX;
  strcpy(ssockaddr.sun_path, "/tmp/r2kd.sock");
  rc = connect(client_sock, (struct sockaddr *) &ssockaddr, len);
  if (rc == -1) {
    log_format(10, stderr, "Could not connect to the server! [%m]\n");
    die(1);
  }
  log_string(0, "Connected to the server!\n", stdout);
}

void shutdown_server_connection() {
  shutdown(client_sock, SHUT_RDWR);
  unlink("/tmp/r2k.client");
}

int main(int argc, char **argv) {
  setlocale(LC_ALL, "");
  set_logging_level(10);

  setup_server_connection();

  int32_t screen;
  Display *dpy;
  XWindowAttributes wa;

  if (!(dpy = XOpenDisplay(NULL))) {
    log_string(10, "Could not open the display!\n", stderr);
    die(1);
  }

  screen = DefaultScreen(dpy);
  root = RootWindow(dpy, screen);

  parentwin = focused_window(dpy);
  log_format(0, stdout, "Parent: 0x%lx\n", parentwin);
  log_format(0, stdout, "Root: 0x%lx\n", root);

  if (!XGetWindowAttributes(dpy, root, &wa)) {
    log_string(10, "Could not get root window attributes!\n", stderr);
    die(1);
  }

  if (!XGetWindowAttributes(dpy, parentwin, &wa)) {
    log_format(10, stderr, "Could not get embedding window attributes: 0x%lx", parentwin);
    die(1);
  }

  drw = drw_create(dpy, screen, wa.width, wa.height);

  if (!drw_fontset_create(use_fonts, sizeof(use_fonts) / sizeof(use_fonts[0]))) {
    log_string(10, "Fonts could not be loaded", stderr);
    die(1);
  }

  lrpad = drw->fonts->h;

  keyboard_grab(drw->dpy);

  /*strcpy(text, "jisho");
  update_results();
  {
    int32_t i;
    for(i = 0; i < res_count; ++i) {
      fprintf(stdout, "[%s]\n", results[i]);
    }
  }*/

  setup();
  run();

  shutdown_server_connection();
  return 0;
}
