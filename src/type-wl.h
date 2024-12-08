#include <stdio.h>
#include <stdint.h>
#include <X11/Xlib.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <xkbcommon/xkbcommon.h>
#include <wayland-client.h>
#include "protocol/kb-protocol.h"
#include "log.h"

uint8_t type_str_wl(char *__restrict str);
