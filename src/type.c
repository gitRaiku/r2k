#include "type.h"

struct cstate {
	struct wl_display *__restrict dpy;
	struct wl_registry *__restrict reg;
	struct wl_seat *__restrict seat;
  
	struct zwp_virtual_keyboard_manager_v1 *__restrict mgr;
	struct zwp_virtual_keyboard_v1 *__restrict kb;
};
struct cstate state;
#define WLCHECK(x,e) {if(!(x)){fputs("Error running " #x " " e "\n", stderr); exit(1);}}

void ckmap(wchar_t *__restrict str, xkb_keysym_t *__restrict kmap, uint32_t sl) {
  int32_t i;
  
	char fname[] = "/tmp/r2ktype-XXXXXX";
  int32_t fd;
	WLCHECK((fd=mkstemp(fname))>=0,"Failed creating the unnecessary external XKB keymap file for Wayland since it decided to use XKB? Why did it use XKB? Could it not have chosen something else? Is it really that hard to make a simpler protocol? Why do i have to bash my head against this X brick wall again? I thought i'd get rid of the problems of X by switching to Wayland, why have they returned?");
	unlink(fname);
	FILE *__restrict f = fdopen(fd, "w");

	fprintf(f,"xkb_keymap {\nxkb_keycodes \"Raiku\" {\nminimum = 8;\nmaximum = %u;\n", sl + 8 + 1);
	fprintf(stdout,"xkb_keymap {\nxkb_keycodes \"Raiku\" {\nminimum = 8;\nmaximum = %u;\n", sl + 8 + 1);
	for (i = 1; i <= sl + 1; ++i) {
		fprintf(f, "<K%u> = %u;\n", i, i + 8);
		fprintf(stdout, "<K%u> = %u;\n", i, i + 8);
	}
	fprintf(f, "};\nxkb_types \"Raiku\" { include \"complete\" };\nxkb_compatibility \"Raiku\" { include \"complete\" };\nxkb_symbols \"Raiku\" {\n");
	fprintf(stdout, "};\nxkb_types \"Raiku\" { include \"complete\" };\nxkb_compatibility \"Raiku\" { include \"complete\" };\nxkb_symbols \"Raiku\" {\n");

  {
    char sn[256];
    fprintf(f, "key <K1> {[NoSymbol]};\n");
    fprintf(stdout, "key <K1> {[NoSymbol]};\n");
    for (i = 0; i < sl; ++i) {
      WLCHECK(xkb_keysym_get_name(kmap[i], sn, sizeof(sn)),"Could not retrieve xkb symbol name for something, idk!");
      fprintf(f, "key <K%u> {[%s]};\n", i + 2, sn);
      fprintf(stdout, "key <K%u> {[%s]};\n", i + 2, sn);
    }
  }

	fputs("};\n};\n", f);
	fputs("};\n};\n", stdout);
	fflush(f);

	zwp_virtual_keyboard_v1_keymap(state.kb, WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1, fd, ftell(f));
	wl_display_roundtrip(state.dpy);
  fclose(f);
}

uint8_t type_str(char *__restrict str) {
  /*
  char ba[1024];
  snprintf(ba, sizeof(ba), "echo \"%s\" | wl-copy", str);
  if (system(ba)) {
    fprintf(stderr, "Error copying\n");
  }
  return 0;
  */

  wchar_t a[1024];
  int32_t i;
  uint32_t al = mbstowcs(a, str, strlen(str));
  xkb_keysym_t kmap[1024] = {0};
  for(i = 0; i < al; ++i) {
    kmap[i] = xkb_utf32_to_keysym(a[i]);
  }

  fprintf(stdout, "Typing %s\n", str);
  ckmap(a, kmap, al);
  
  zwp_virtual_keyboard_v1_key(state.kb, 0, 1, WL_KEYBOARD_KEY_STATE_PRESSED); wl_display_roundtrip(state.dpy);
  zwp_virtual_keyboard_v1_key(state.kb, 0, 1, WL_KEYBOARD_KEY_STATE_RELEASED); wl_display_roundtrip(state.dpy);

  struct timespec ts = { .tv_sec = 0, .tv_nsec = 10000000  };
  for(i = 2; i <= al + 1; ++i) {
    zwp_virtual_keyboard_v1_key(state.kb, 0, i, WL_KEYBOARD_KEY_STATE_PRESSED); wl_display_roundtrip(state.dpy);
    zwp_virtual_keyboard_v1_key(state.kb, 0, i, WL_KEYBOARD_KEY_STATE_RELEASED); wl_display_roundtrip(state.dpy);
  }

  return 0;
}

void eventhand(void *__restrict data, struct wl_registry *__restrict reg,
							        uint32_t name, const char *__restrict iface,
							        uint32_t version) {
	struct cstate *__restrict state = data;
	if (!strcmp(iface, wl_seat_interface.name)) {
		state->seat = wl_registry_bind(reg, name, &wl_seat_interface, version <= 7 ? version : 7);
	} else if (!strcmp(iface, zwp_virtual_keyboard_manager_v1_interface.name)) {
		state->mgr = wl_registry_bind(reg, name, &zwp_virtual_keyboard_manager_v1_interface, 1);
	}
}

void eventremhand(void *data, struct wl_registry *registry, uint32_t name) { return; }

const struct wl_registry_listener reglistener = {
	.global = eventhand,
	.global_remove = eventremhand,
};

void wlstart() {
  memset(&state, 0, sizeof(state));

  WLCHECK(state.dpy=wl_display_connect(NULL),"Could not connect to the wayland display!");
  WLCHECK(state.reg=wl_display_get_registry(state.dpy),"Could not get the wayland registry!");
	wl_registry_add_listener(state.reg, &reglistener, &state);
	wl_display_dispatch(state.dpy);
	wl_display_roundtrip(state.dpy);

  WLCHECK(state.mgr,"The wayland compositor does not support the virtual keyboard protocol!");
  WLCHECK(state.seat,"Could not find any valid wayland seat!");

	state.kb = zwp_virtual_keyboard_manager_v1_create_virtual_keyboard(state.mgr, state.seat);
}

void wlend() {
	zwp_virtual_keyboard_v1_destroy(state.kb);
	zwp_virtual_keyboard_manager_v1_destroy(state.mgr);
	wl_registry_destroy(state.reg);
	wl_display_disconnect(state.dpy);
}
