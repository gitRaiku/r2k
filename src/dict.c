#include "dict.h"

struct dictEntry *dict;
uint32_t dictLen = 0;

void read_uint64_t(uint64_t *__restrict nr, char *str, size_t *spos) {
  uint8_t ch;
  *nr = 0;
  *spos = 0;
  while ((ch = str[*spos]) && ('0' <= ch && ch <= '9')) {
    *nr *= 10;
    *nr += ch - '0';
    ++*spos;
  }
}

void read_uint32_t(FILE *__restrict stream, uint32_t *__restrict nr) {
  uint8_t ch;
  *nr = 0;
  while ((ch = fgetc(stream)) && ('0' <= ch && ch <= '9')) {
    *nr *= 10;
    *nr += ch - '0';
  }
  if (ch == '\r') {
    fgetc(stream);
  }
}

uint8_t dict_init() {
  dictLen = 0;
  dict = calloc(DICT_ENTRIES, sizeof(struct dictEntry));

  {
    int32_t fd = open(DICTPATH1, O_RDONLY);
    if (fd == -1) {
      fd = open(DICTPATH2, O_RDONLY);
      if (fd == -1) {
        dict_destroy();
        return 1;
      }
    }

    char buf[1024];
    size_t cl = 0;
    size_t pos;
    ssize_t readl = 0;
    uint64_t hash;
    while ((readl = read(fd, buf+cl, 1))) {
      if (buf[cl] == '\n') {
        read_uint64_t(&hash, buf, &pos);
        dict[dictLen].hash = hash;
        dict[dictLen].mana = malloc(sizeof(char) * (cl - pos));
        strncpy(dict[dictLen].mana, buf+pos+1, cl-pos-1);
        dict[dictLen].mana[cl-pos-1] = '\0';
        ++dictLen;
        cl = -1;
      }
      ++cl;
    }
  }
  return 0;
}

void dict_destroy() {
  uint32_t i;
  for(i = 0; i < dictLen; ++i) {
    free(dict[i].mana);
  }
  free(dict);
}
