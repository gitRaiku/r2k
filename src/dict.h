#ifndef DICT_H
#define DICT_H

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include "defs.h"

struct dictEntry {
  uint64_t hash;
  char *mana;
};
extern struct dictEntry *dict;
extern uint32_t dictLen;

uint8_t dict_init();
void dict_destroy();

#endif
