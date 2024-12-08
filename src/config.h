#ifndef R2K_CONFIG_H
#define R2K_CONFIG_H

#define TYPE_WL 1
#define TYPE_XORG 1

enum {
    SchemeNorm, SchemeSel, SchemeOut, SchemeLast
}; /* color schemes */

const char *colors[SchemeLast][2] = {
        /*     fg         bg       */
        [SchemeNorm] = {"#bbb8c0", "#151717"},
        [SchemeSel]  = {"#111513", "#936ab0"},
        [SchemeOut]  = {"#111513", "#936ab0"},
};

const char *use_fonts[] = { "JetBrainsMono:size=18" };

const uint32_t log_level = 10; /// 0 for all logging, 10 for errors

#endif //R2K_CONFIG_H
