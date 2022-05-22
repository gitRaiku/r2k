#include "type.h"

/*void print_mapping(Display *dpy, char *fname) {
  int l;
  KeySym *__restrict mapping = XGetKeyboardMapping(dpy, 8, 255 - 8, &l);

  int32_t i, j;
  FILE *__restrict f = fopen(fname, "w");

  for (i = 0; i < 255 - 8; ++i) {
    for (j = 0; j < l; ++j) {
      fprintf(f, "%10lu | ", mapping[OFF(l, i, j)]);
    }
    fputc('\n', f);
  }

  fclose(f);
}*/

uint32_t first_empty(KeySym *__restrict mapping, uint32_t l) {
  uint32_t r = 0;
  int32_t i, j;
  for (i = 0; i < 255 - 8; ++i) {
    r = 0;
    for (j = 0; j < l; ++j) {
      r |= mapping[OFF(l, i, j)];
    }
    if (r == 0) {
      return i;
    }
  }
  return -1;
}

uint32_t runel(char *__restrict str) {
  char *__restrict os = str;
  for (++str; (*str & 0xc0) == 0x80; ++str);
  return (uint32_t) (str - os);
}

uint32_t utf8_to_unicode(char *__restrict str, uint32_t l) {
  uint32_t res = 0;
  switch (l) {
    case 4:
      res |= *str & 0x7;
      break;
    case 3:
      res |= *str & 0xF;
      break;
    case 2:
      res |= *str & 0x1F;
      break;
    case 1:
      res |= *str & 0x7F;
      break;
  }

  --l;
  while (l) {
    ++str;
    res <<= 6;
    res |= *str & 0x3F;
    --l;
  }

  return res;
}

uint8_t type_str(char *__restrict str) {
  Display *dpy;
  dpy = XOpenDisplay(NULL);
  int32_t width;
  KeySym *__restrict mapping = XGetKeyboardMapping(dpy, 8, 255 - 8, &width);

  uint32_t fe = first_empty(mapping, width);
  if (fe == -1) {
    fputs("Could not find an empty slot in the current keyboard mapping!", stderr);
    return 1;
  }

  Window root;
  int screen;
  screen = DefaultScreen(dpy);
  root = RootWindow(dpy, screen);


  Window window;
  int revertToReturn;
  XGetInputFocus(dpy, &window, &revertToReturn);

  uint32_t cl;
  uint32_t uc;
  XKeyEvent ev;
  ev.type = 0;
  ev.serial = 0;
  ev.send_event = 0;
  ev.display = dpy;
  ev.window = window;
  ev.root = root;
  ev.subwindow = 0;
  ev.time = 0;
  ev.x = ev.y = 0;
  ev.x_root = ev.y_root = 0;
  ev.state = 0;
  ev.keycode = 0;
  ev.same_screen = 0;

  XSync(dpy, True);
  XSync(dpy, True);
  XSync(dpy, True);
  while (*str) {
    cl = runel(str);

    uc = utf8_to_unicode(str, cl) | FUCK_YOU_XORG_WHY_DID_YOU_MAKE_ME_HAVE_TO_JUST_RANDOMLY_OR_WITH_THIS_RANDOM_BIT_JUST_SO_YOU_DONT_HAVE_PROBLEMS_WITH_OVERLAP_KILL_YOURSELF_NOW_BIT;

    mapping[OFF(width, fe, 0)] = uc;

    XChangeKeyboardMapping(dpy, fe + 8, width, mapping + OFF(width, fe, 0), 1);
    XSync(dpy, True);
    XSync(dpy, True);
    XSync(dpy, True);

    ev.keycode = fe + 8;

    ev.type = KeyPress;
    XSendEvent(dpy, window, False, 0, (XEvent *) &ev);
    XSync(dpy, True);
    XSync(dpy, True);
    XSync(dpy, True);

    ev.type = KeyRelease;
    XSendEvent(dpy, window, False, 0, (XEvent *) &ev);
    XSync(dpy, True);
    XSync(dpy, True);
    XSync(dpy, True);

    str += cl;
  }

  mapping[OFF(width, fe, 0)] = 0;
  XChangeKeyboardMapping(dpy, fe + 8, width, mapping + OFF(width, fe, 0), 1);
  XSync(dpy, False);

  return 0;
}
