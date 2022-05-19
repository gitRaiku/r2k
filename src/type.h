#include <stdio.h>
#include <stdint.h>
#include <X11/Xlib.h>
#include <string.h>

#define OFF(l, x, y) ((x) * (l) + (y))

#define FUCK_YOU_XORG_WHY_DID_YOU_MAKE_ME_HAVE_TO_JUST_RANDOMLY_OR_WITH_THIS_RANDOM_BIT_JUST_SO_YOU_DONT_HAVE_PROBLEMS_WITH_OVERLAP_KILL_YOURSELF_NOW_BIT 0b1000000000000000000000000

uint8_t type_str(char *__restrict str);
