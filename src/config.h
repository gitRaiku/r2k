#ifndef R2K_CONFIG_H
#define R2K_CONFIG_H

enum {
    SchemeNorm, SchemeSel, SchemeOut, SchemeLast
}; /* color schemes */

const char *colors[SchemeLast][2] = {
        /*     fg         bg       */
        [SchemeNorm] = {"#a6c2e8", "#090408"},
        [SchemeSel]  = {"#a6c2e8", "#99210A"},
        [SchemeOut]  = {"#a6c2e8", "#3147C7"},
};

const char *use_fonts[] = {
        "JetBrainsMono:size=18"
};

#endif //R2K_CONFIG_H
