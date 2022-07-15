#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <errno.h>
#include <signal.h>
#include <locale.h>

#include "log.h"
#include "dict.h"

volatile uint8_t run;
sig_atomic_t recv_sig = 0;

#define SOCK_PATH "/tmp/r2kd.sock"
FILE *__restrict log_file;

static void interrupt_handler(sig_atomic_t sig) {
  run = 0;
  recv_sig = sig;
}

//                          。      ぁ      あ      ぃ      い      ぅ      う      ぇ      え      ぉ      
uint16_t transformMap[] = { 0x0000, 0x8181, 0x8182, 0x8183, 0x8184, 0x8185, 0x8186, 0x8187, 0x8188, 0x8189, 
//                          お      か      が      き      ぎ      く      ぐ      け      げ      こ      
                            0x818a, 0x818b, 0x818c, 0x818d, 0x818e, 0x818f, 0x8190, 0x8191, 0x8192, 0x8193, 
//                          ご      さ      ざ      し      じ      す      ず      せ      ぜ      そ      
                            0x8194, 0x8195, 0x8196, 0x8197, 0x8198, 0x8199, 0x819a, 0x819b, 0x819c, 0x819d, 
//                          ぞ      た      だ      ち      ぢ      っ      つ      づ      て      で      
                            0x819e, 0x819f, 0x81a0, 0x81a1, 0x81a2, 0x81a3, 0x81a4, 0x81a5, 0x81a6, 0x81a7, 
//                          と      ど      な      に      ぬ      ね      の      は      ば      ぱ      
                            0x81a8, 0x81a9, 0x81aa, 0x81ab, 0x81ac, 0x81ad, 0x81ae, 0x81af, 0x81b0, 0x81b1, 
//                          ひ      び      ぴ      ふ      ぶ      ぷ      へ      べ      ぺ      ほ      
                            0x81b2, 0x81b3, 0x81b4, 0x81b5, 0x81b6, 0x81b7, 0x81b8, 0x81b9, 0x81ba, 0x81bb, 
//                          ぼ      ぽ      ま      み      む      め      も      ゃ      や      ゅ      
                            0x81bc, 0x81bd, 0x81be, 0x81bf, 0x8280, 0x8281, 0x8282, 0x8283, 0x8284, 0x8285, 
//                          ゆ      ょ      よ      ら      り      る      れ      ろ      わ      を      
                            0x8286, 0x8287, 0x8288, 0x8289, 0x828a, 0x828b, 0x828c, 0x828d, 0x828f, 0x8292, 
//                          ん      ー      ゎ      ゐ      ゑ      ゕ      ゖ      ゔ      ゝ      ゞ      
                            0x8293, 0x83bc, 0x828e, 0x8290, 0x8291, 0x8295, 0x8296, 0x8294, 0x829d, 0x829e, 
//                          ・      「      」      。      、            
                            0x83bb, 0x808c, 0x808d, 0x8082, 0x8081 };

char *hashToKata[] =  { "。", "ァ", "ア", "ィ", "イ", "ゥ", "ウ", "ェ", "エ", "ォ",
                        "オ", "カ", "ガ", "キ", "ギ", "ク", "グ", "ケ", "ゲ", "コ",
                        "ゴ", "サ", "ザ", "シ", "ジ", "ス", "ズ", "セ", "ゼ", "ソ",
                        "ゾ", "タ", "ダ", "チ", "ヂ", "ッ", "ツ", "ヅ", "テ", "デ",
                        "ト", "ド", "ナ", "ニ", "ヌ", "ネ", "ノ", "ハ", "バ", "パ",
                        "ヒ", "ビ", "ピ", "フ", "ブ", "プ", "ヘ", "ベ", "ペ", "ホ",
                        "ボ", "ポ", "マ", "ミ", "ム", "メ", "モ", "ャ", "ヤ", "ュ",
                        "ユ", "ョ", "ヨ", "ラ", "リ", "ル", "レ", "ロ", "ワ", "ヲ",
                        "ン", "ー", "ヮ", "ヰ", "ヱ", "ゕ", "ゖ", "ゔ", "ゝ", "ゞ",
                        "・", "「", "」", "。", "、" };

uint64_t stringToHash(char *str) {
  int32_t strl = strlen(str);
  /** All characters start ith 0xe3 **/
  uint16_t cc = 0;
  uint64_t i, j;
  uint64_t cpos = 1lu<<57lu;
  uint64_t res = 0;
  for(i = 0; i < strl; i += 3) { 
    cc = (uint8_t)str[i+1]<<8 | (uint8_t)str[i+2];
    for(j = 0; j < sizeof(transformMap)/sizeof(transformMap[0]); ++j) {
      if (cc == transformMap[j]) {
        res |= cpos * j;
        cpos >>= 7;
        break;
      }
    }
  }
  return res;
}

void hexprint(char *buf, uint32_t l) {
  int32_t i;
  for(i = 0; i < l; ++i) {
    fprintf(log_file, "0x%x ", buf[i]);
  }
  fputc('\n', log_file);
}

/**
 *
 * Communication protocol:
 * Client -> Server
 * [8bits]  [16bits] [len * 8bits]
 * Function Length   Characters
 *
 * Romaji to hiragana:
 *  function = 0b01
 * Romaji to hash:
 *  function = 0b10
 * Romaji to dict entries:
 *  function = 0b11
 *
 * Client <- Server
 *  Romaji to hiragana:
 *    [8bits]  [16bits] [len * 8bits]
 *    Function Length   Characters
 *
 *    Function = 0b01
 *  Romaji to hash:
 *    [8bits]  [64bits]
 *    Function Hash
 *
 *    Function = 0b01
 *  Romaji to dict entries:
 *    [8bits]  [32bits] [len * 8bits]
 *    Function Length   Entries broken up by 0x0a('\n')
 **/

enum functions {
  FUNC_NONE, FUNC_R2G, FUNC_R2H, FUNC_R2D
};

void str_append(char *__restrict buf, uint32_t *__restrict len, char *__restrict str) {
  uint32_t strl = strlen(str);
  memcpy(buf + *len, str, strl);
  *len += strl; 
}

void r2g(char *__restrict buf, uint32_t bl, char *__restrict romajiBuf, uint32_t *__restrict rl) {
  *rl = 0;
  int32_t cl = 0;
  while (cl < bl) {
    if (buf[cl] == buf[cl+1]) {
      if (buf[cl] != 'a' && buf[cl] != 'e' && buf[cl] != 'i' && buf[cl] != 'o' && buf[cl] != 'u') {
        if (buf[cl] == 'n') {
          str_append(romajiBuf, rl, "ん");
        } else {
          str_append(romajiBuf, rl, "っ");
        }
        ++cl;
        continue;
      }
    }
    switch (buf[cl]) {
      case 'a':
        str_append(romajiBuf, rl, "あ");
        ++cl;
        break;
      case 'i':
        str_append(romajiBuf, rl, "い");
        ++cl;
        break;
      case 'u':
        str_append(romajiBuf, rl, "う");
        ++cl;
        break;
      case 'e':
        str_append(romajiBuf, rl, "え");
        ++cl;
        break;
      case 'o':
        str_append(romajiBuf, rl, "お");
        ++cl;
        break;
      case 'k':
        switch (buf[cl+1]) {
          case 'y':
            switch (buf[cl+2]) {
              case 'a':
                str_append(romajiBuf, rl, "きゃ");
                break;
              case 'u':
                str_append(romajiBuf, rl, "きゅ");
                break;
              case 'o':
                str_append(romajiBuf, rl, "きょ");
                break;
            }
            ++cl;
            break;
          case 'a':
            str_append(romajiBuf, rl, "か");
            break;
          case 'i':
            str_append(romajiBuf, rl, "き");
            break;
          case 'u':
            str_append(romajiBuf, rl, "く");
            break;
          case 'e':
            str_append(romajiBuf, rl, "け");
            break;
          case 'o':
            str_append(romajiBuf, rl, "こ");
            break;
        }
        cl += 2;
        break;
      case 'g':
        switch (buf[cl+1]) {
          case 'y':
            switch (buf[cl + 2]) {
              case 'o':
                str_append(romajiBuf, rl, "ぎょ");
                break;
              case 'u':
                str_append(romajiBuf, rl, "ぎゅ");
                break;
              case 'a':
                str_append(romajiBuf, rl, "ぎゃ");
                break;
            }
            ++cl;
            break;
          case 'a':
            str_append(romajiBuf, rl, "が");
            break;
          case 'i':
            str_append(romajiBuf, rl, "ぎ");
            break;
          case 'u':
            str_append(romajiBuf, rl, "ぐ");
            break;
          case 'e':
            str_append(romajiBuf, rl, "げ");
            break;
          case 'o':
            str_append(romajiBuf, rl, "ご");
            break;
        }
        cl += 2;
        break;
      case 'c':
        switch (buf[cl+2]) {
          case 'i':
            str_append(romajiBuf, rl, "ち");
            break;
          case 'a':
            str_append(romajiBuf, rl, "ちゃ");
            break;
          case 'u':
            str_append(romajiBuf, rl, "ちゅ");
            break;
          case 'o':
            str_append(romajiBuf, rl, "ちょ");
            break;
          case 'e':
            str_append(romajiBuf, rl, "ちぇ");
            break;
        }
        cl += 3;
        break;
      /** No ぢ because fuck you **/
      case 's':
        switch (buf[cl+1]) {
          case 'h':
            switch (buf[cl+2]) {
              case 'i':
                str_append(romajiBuf, rl, "し");
                break;
              case 'a':
                str_append(romajiBuf, rl, "しゃ");
                break;
              case 'u':
                str_append(romajiBuf, rl, "しゅ");
                break;
              case 'o':
                str_append(romajiBuf, rl, "しょ");
                break;
            }
            ++cl;
            break;
          case 'a':
            str_append(romajiBuf, rl, "さ");
            break;
          case 'u':
            str_append(romajiBuf, rl, "す");
            break;
          case 'e':
            str_append(romajiBuf, rl, "せ");
            break;
          case 'o':
            str_append(romajiBuf, rl, "そ");
            break;
        }
        cl += 2;
        break;
      case 'z':
        switch (buf[cl+1]) {
          case 'a':
            str_append(romajiBuf, rl, "ざ");
            break;
          case 'u':
            str_append(romajiBuf, rl, "ず");
            break;
          case 'e':
            str_append(romajiBuf, rl, "ぜ");
            break;
          case 'o':
            str_append(romajiBuf, rl, "ぞ");
            break;
        }
        cl += 2;
        break;
      case 'j':
        switch (buf[cl+1]) {
          case 'i':
            str_append(romajiBuf, rl, "じ");
            break;
          case 'a':
            str_append(romajiBuf, rl, "じゃ");
            break;
          case 'u':
            str_append(romajiBuf, rl, "じゅ");
            break;
          case 'o':
            str_append(romajiBuf, rl, "じょ");
            break;
          default:
            --cl;
            break;
        }
        cl += 2;
        break;
      case 't':
        switch (buf[cl+1]) {
          case 's':
            str_append(romajiBuf, rl, "つ");
            ++cl;
            break;
          case 'a':
            str_append(romajiBuf, rl, "た");
            break;
          case 'e':
            str_append(romajiBuf, rl, "て");
            break;
          case 'o':
            str_append(romajiBuf, rl, "と");
            break;
          case 'i':
            str_append(romajiBuf, rl, "てぃ");
            break;
          case 'u':
            str_append(romajiBuf, rl, "てぅ");
            break;
        }
        cl += 2;
        break;
      case 'd':
        switch (buf[cl+1]) {
          case 'z':
            str_append(romajiBuf, rl, "づ");
            ++cl;
            break;
          case 'a':
            str_append(romajiBuf, rl, "だ");
            break;
          case 'e':
            str_append(romajiBuf, rl, "で");
            break;
          case 'o':
            str_append(romajiBuf, rl, "ど");
            break;
          case 'i':
            str_append(romajiBuf, rl, "でぃ");
            break;
          case 'u':
            str_append(romajiBuf, rl, "でぅ");
            break;
        }
        cl += 2;
        break;
      case 'n':
        switch (buf[cl+1]) {
          case '\\':
            str_append(romajiBuf, rl, "ん");
            --cl;
            break;
          case 'y':
            switch (buf[cl + 2]) {
              case 'o':
                str_append(romajiBuf, rl, "にょ");
                break;
              case 'u':
                str_append(romajiBuf, rl, "にゅ");
                break;
              case 'a':
                str_append(romajiBuf, rl, "にゃ");
                break;
            }
            ++cl;
            break;
          case 'a':
            str_append(romajiBuf, rl, "な");
            break;
          case 'e':
            str_append(romajiBuf, rl, "ね");
            break;
          case 'o':
            str_append(romajiBuf, rl, "の");
            break;
          case 'i':
            str_append(romajiBuf, rl, "に");
            break;
          case 'u':
            str_append(romajiBuf, rl, "ぬ");
            break;
          default:
            str_append(romajiBuf, rl, "ん");
            --cl;
            break;
        }
        cl += 2;
        break;
      case 'h':
        switch (buf[cl+1]) {
          case 'y':
            switch (buf[cl + 2]) {
              case 'o':
                str_append(romajiBuf, rl, "ひょ");
                break;
              case 'u':
                str_append(romajiBuf, rl, "ひゅ");
                break;
              case 'a':
                str_append(romajiBuf, rl, "ひゃ");
                break;
            }
            ++cl;
            break;
          case 'a':
            str_append(romajiBuf, rl, "は");
            break;
          case 'e':
            str_append(romajiBuf, rl, "へ");
            break;
          case 'i':
            str_append(romajiBuf, rl, "ひ");
            break;
          case 'o':
            str_append(romajiBuf, rl, "ほ");
            break;
        }
        cl += 2;
        break;
      case 'b':
        switch (buf[cl+1]) {
          case 'a':
            str_append(romajiBuf, rl, "ば");
            break;
          case 'e':
            str_append(romajiBuf, rl, "べ");
            break;
          case 'u':
            str_append(romajiBuf, rl, "ぶ");
            break;
          case 'i':
            str_append(romajiBuf, rl, "び");
            break;
          case 'o':
            str_append(romajiBuf, rl, "ぼ");
            break;
        }
        cl += 2;
        break;
      case 'p':
        switch (buf[cl+1]) {
          case 'y':
            switch (buf[cl+2]) {
              case 'a':
                str_append(romajiBuf, rl, "ぴゃ");
                break;
              case 'u':
                str_append(romajiBuf, rl, "ぴゅ");
                break;
              case 'o':
                str_append(romajiBuf, rl, "ぴょ");
                break;
            }
            ++cl;
            break;
          case 'a':
            str_append(romajiBuf, rl, "ぱ");
            break;
          case 'e':
            str_append(romajiBuf, rl, "ぺ");
            break;
          case 'u':
            str_append(romajiBuf, rl, "ぷ");
            break;
          case 'i':
            str_append(romajiBuf, rl, "ぴ");
            break;
          case 'o':
            str_append(romajiBuf, rl, "ぽ");
            break;
        }
        cl += 2;
        break;
      case 'f':
        switch (buf[cl+1]) {
          case 'u':
            str_append(romajiBuf, rl, "ふ");
            break;
          case 'o':
            str_append(romajiBuf, rl, "ふぉ");
            break;
        }
        cl += 2;
        break;
      case 'm':
        switch (buf[cl+1]) {
          case 'y':
            switch (buf[cl + 2]) {
              case 'o':
                str_append(romajiBuf, rl, "みょ");
                break;
              case 'u':
                str_append(romajiBuf, rl, "みゅ");
                break;
              case 'a':
                str_append(romajiBuf, rl, "みゃ");
                break;
            }
            ++cl;
            break;
          case 'a':
            str_append(romajiBuf, rl, "ま");
            break;
          case 'i':
            str_append(romajiBuf, rl, "み");
            break;
          case 'u':
            str_append(romajiBuf, rl, "む");
            break;
          case 'e':
            str_append(romajiBuf, rl, "め");
            break;
          case 'o':
            str_append(romajiBuf, rl, "も");
            break;
        }
        cl += 2;
        break;
      case 'y':
        switch (buf[cl+1]) {
          case 'a':
            str_append(romajiBuf, rl, "や");
            break;
          case 'u':
            str_append(romajiBuf, rl, "ゆ");
            break;
          case 'o':
            str_append(romajiBuf, rl, "よ");
            break;
        }
        cl += 2;
        break;
      case 'r':
        switch (buf[cl+1]) {
          case 'y':
            switch (buf[cl + 2]) {
              case 'o':
                str_append(romajiBuf, rl, "りょ");
                break;
              case 'u':
                str_append(romajiBuf, rl, "りゅ");
                break;
              case 'a':
                str_append(romajiBuf, rl, "りゃ");
                break;
            }
            ++cl;
            break;
          case 'a':
            str_append(romajiBuf, rl, "ら");
            break;
          case 'i':
            str_append(romajiBuf, rl, "り");
            break;
          case 'u':
            str_append(romajiBuf, rl, "る");
            break;
          case 'e':
            str_append(romajiBuf, rl, "れ");
            break;
          case 'o':
            str_append(romajiBuf, rl, "ろ");
            break;
        }
        cl += 2;
        break;
      case 'w':
        switch (buf[cl+1]) {
          case 'a':
            str_append(romajiBuf, rl, "わ");
            break;
          case 'o':
            str_append(romajiBuf, rl, "を");
            break;
        }
        cl += 2;
        break;
      default:
        ++cl;
        break;
    }
  }
  romajiBuf[*rl] = '\0';
  ++*rl;
}

uint32_t get_hash_pos(uint64_t hash) {
  log_format(0, log_file, "Starting search for hash %lu\n", hash);
  uint32_t cpos;
  int32_t lbound = 0;
  int32_t ubound = dictLen;
  uint32_t answer = 0;
  while (lbound <= ubound) {
    cpos = (ubound + lbound) / 2;
    log_format(0, log_file, "lb: %u; ub: %u; cp: %u; cp.hash %lu\n", lbound, ubound, cpos, dict[cpos].hash);
    if (dict[cpos].hash < hash) {
      lbound = cpos + 1;
    } else {
      ubound = cpos - 1;
      answer = cpos;
    }
  }
  return answer;
}

void get_dict(uint64_t hash, uint32_t *__restrict pos, uint32_t *__restrict epos) {
  log_string(0, "G_DICT\n", log_file);
  *pos = get_hash_pos(hash);
  log_string(0, "G_HASH\n", log_file);
  *epos = *pos;
  while (dict[*epos].hash == dict[*pos].hash) { /** Position of last hash + 1 **/
    ++*epos; 
  }
  int32_t i;
  log_format(0, log_file, "Got position: %u - %u\n", *pos, *epos);
  for(i = *pos; i < *epos; ++i) {
    log_format(0, log_file, "\t%lu %s\n", dict[i].hash, dict[i].mana);
  }
}

void __inline__ make_empty_packet(char *response, uint32_t *rl) {
  response[0] = 
  response[1] = 
  response[2] = 
  response[3] = 0x00;
  *rl = 4;
}

void hash_to_kata(uint64_t hash, char *kata, uint32_t *kl) {
  *kl = 0;
  int64_t cpos = 57;
  char *cglyph;
  while (cpos > 0) {
    fprintf(stdout, "%lu: %lu %lu %lu %lu\n", cpos, hash, 0x7Flu << cpos, hash & (0x7Flu << cpos), (hash & (0x7Flu << cpos)) >> cpos);
    cglyph = hashToKata[(hash & (0x7Flu << cpos)) >> cpos];
    if (cglyph == *hashToKata) {
      break;
    }
    strcpy(kata + *kl, cglyph);
    *kl += strlen(cglyph);
    cpos -= 7;
  }
  kata[*kl] = '\0';
}

void interpret_data(char *buf, int32_t len, char *response, uint32_t *rl) {
  uint8_t function;
  uint16_t chlen;
  function = (uint8_t)buf[0];
  chlen = (uint8_t)buf[1] << 8u | (uint8_t)buf[2];
  log_format(0, log_file, "Chlen: %u\n", chlen);

  if (chlen == 0) {
    log_string(0, "Got an empty packet, sending one back!\n", log_file);
    make_empty_packet(response, rl);
    return;
  }

  if (chlen + 3 != len) {
    log_format(0, log_file, "Length mismatch! Got %i, expected %hu\n", len, chlen + 3);
    make_empty_packet(response, rl);
    hexprint(buf, len);
  }

  char ganabuf[512] = {0};
  uint32_t ganal;
  uint64_t hash;
  uint32_t spos, epos;
  uint32_t cl = 0;

  switch (function) {
    case FUNC_R2G:
      log_string(0, "Romaji to hiragana\n", log_file);
      log_format(0, log_file, "[%s] %u\n", buf+3, chlen);
      r2g(buf+3, chlen, ganabuf, &ganal);
      response[0] = 0b01;
      response[1] = ganal & 0xFF00;
      response[2] = ganal & 0x00FF;
      log_format(0, log_file, "[%s] %u\n", ganabuf, ganal);
      memcpy(response + 3, ganabuf, ganal);
      *rl = 3 + ganal;
      break;
    case FUNC_R2H:
      log_string(0, "Romaji to hash\n", log_file);
      r2g(buf+3, chlen, ganabuf, &ganal);
      hash = stringToHash(ganabuf);
      log_format(0, log_file, "Got [%s] with hash %lu\n", ganabuf, hash);
      response[0] = 0b10;
      response[1] = 0x00;
      response[2] = 0x8;
      memcpy(response + 3, &hash, sizeof(hash));
      *rl = 11;
      break;
    case FUNC_R2D:
      log_string(0, "Romaji to dict\n", log_file);
      switch (buf[3]) {
#define PUSH_CHAR(ch)                    \
          strcpy(response + 3 + cl, ch); \
          cl += strlen(ch) + 1;          \
          response[2 + cl] = '\n';
        case 'S':
          PUSH_CHAR("ß");
          PUSH_CHAR("ß");
          break;
        case ':':
          switch (buf[4]) {
            case 'a':
              PUSH_CHAR("ä");
              PUSH_CHAR("Ä");
              break;
            case 'e':
              PUSH_CHAR("ë");
              PUSH_CHAR("Ë");
              break;
            case 'u':
              PUSH_CHAR("ü");
              PUSH_CHAR("Ü");
              break;
            case 'o':
              PUSH_CHAR("ö");
              PUSH_CHAR("Ö");
              break;
            case 'i':
              PUSH_CHAR("ï");
              PUSH_CHAR("Ï");
              break;
            default:
              goto end;
          }
          break;
        case ',':
          switch (buf[4]) {
            case 's':
              PUSH_CHAR("ş");
              PUSH_CHAR("Ş");
              break;
            case 't':
              PUSH_CHAR("ţ");
              PUSH_CHAR("Ţ");
              break;
            case 'c':
              PUSH_CHAR("ç");
              PUSH_CHAR("Ç");
              break;
            default:
              goto end;
          }
          break;
        case '^':
          switch (buf[4]) {
            case 'a':
              PUSH_CHAR("â");
              PUSH_CHAR("Â");
              break;
            case 'e':
              PUSH_CHAR("ê");
              PUSH_CHAR("Ê");
              break;
            case 'u':
              PUSH_CHAR("û");
              PUSH_CHAR("Û");
              break;
            case 'o':
              PUSH_CHAR("ô");
              PUSH_CHAR("Ô");
              break;
            case 'i':
              PUSH_CHAR("î");
              PUSH_CHAR("Î");
              break;
            default:
              goto end;
          }
          break;
        case '\'':
          switch (buf[4]) {
            case 'a':
              PUSH_CHAR("á");
              PUSH_CHAR("Á");
              break;
            case 'e':
              PUSH_CHAR("é");
              PUSH_CHAR("É");
              break;
            case 'u':
              PUSH_CHAR("ú");
              PUSH_CHAR("Ú");
              break;
            case 'o':
              PUSH_CHAR("ó");
              PUSH_CHAR("Ó");
              break;
            case 'i':
              PUSH_CHAR("í");
              PUSH_CHAR("Í");
              break;
            default:
              goto end;
          }
          break;
        case '`':
          switch (buf[4]) {
            case 'a':
              PUSH_CHAR("à");
              PUSH_CHAR("À");
              break;
            case 'e':
              PUSH_CHAR("è");
              PUSH_CHAR("È");
              break;
            case 'u':
              PUSH_CHAR("ù");
              PUSH_CHAR("Ù");
              break;
            case 'o':
              PUSH_CHAR("ò");
              PUSH_CHAR("Ò");
              break;
            case 'i':
              PUSH_CHAR("ì");
              PUSH_CHAR("Ì");
              break;
            default:
              goto end;
          }
          break;
        case '(':
          switch (buf[4]) {
            case 'a':
              PUSH_CHAR("ă");
              PUSH_CHAR("Ă");
              break;
            case 'e':
              PUSH_CHAR("ĕ");
              PUSH_CHAR("Ĕ");
              break;
            case 'u':
              PUSH_CHAR("ŭ");
              PUSH_CHAR("Ŭ");
              break;
            case 'o':
              PUSH_CHAR("ŏ");
              PUSH_CHAR("Ŏ");
              break;
            case 'i':
              PUSH_CHAR("ĭ");
              PUSH_CHAR("Ĭ");
              break;
            default:
              goto end;
          }
          break;
        case '.':
          switch (buf[4]) {
            case '(':
            case '[':
            case '{':
              PUSH_CHAR("【");
              PUSH_CHAR("｛");
              PUSH_CHAR("［");
              PUSH_CHAR("（");
              break;
            case ')':
            case ']':
            case '}':
              PUSH_CHAR("】");
              PUSH_CHAR("｝");
              PUSH_CHAR("］");
              PUSH_CHAR("）");
              break;
            case ',':
              PUSH_CHAR("、");
              break;
            case '.':
              PUSH_CHAR("　");
              break;
            case '\'':
              PUSH_CHAR("「");
              PUSH_CHAR("」");
              break;
            case '\"':
              PUSH_CHAR("『");
              PUSH_CHAR("』");
              PUSH_CHAR("〝");
              PUSH_CHAR("〟");
              break;
            case '~':
              PUSH_CHAR("〜");
              break;
            case '-':
              PUSH_CHAR("ー");
              break;
            case 'M':
              PUSH_CHAR("♪");
              break;
            default:
              PUSH_CHAR("。");
              break;
          }
          break;
#undef PUSH_CHAR
        default:
          r2g(buf+3, chlen, ganabuf, &ganal);
          log_format(0, log_file, "Got hiragana %u:[%s]\n", ganal, ganabuf);
          if (ganal == 1) {
            make_empty_packet(response, rl);
            log_string(0, "Couldn't create a hiragana transcription, sending back an empty packet!\n", log_file);
            return;
          }
          hash = stringToHash(ganabuf);
          log_format(0, log_file, "Got hash: %lu\n", hash);
          get_dict(hash, &spos, &epos);
          int32_t i;

          strcpy(response+3, dict[spos].mana);
          cl += strlen(dict[spos].mana) + 1;
          response[2+cl] = '\n';

          strcpy(response+3+cl, ganabuf);
          cl += strlen(ganabuf) + 1;
          response[2+cl] = '\n';

          for(i = spos + 1; i < epos; ++i) {
            strcpy(response+3+cl, dict[i].mana);
            cl += strlen(dict[i].mana) + 1;
            response[2+cl] = '\n';
          }

          uint32_t kl;
          hash_to_kata(hash, response+3+cl, &kl);
          cl += kl + 1;
          response[2+cl] = '\n';
          response[3+cl] = '\0';
          break;
      }

end:;
      log_format(0, log_file, "%u: [%s]\n", cl, response+3);
      response[0] = 0b11;
      response[1] = cl & 0xFF00;
      response[2] = cl & 0x00FF;
      *rl = 3 + cl;
      break;
    default:
      log_string(1, "Got broken packet! Sending empty packet pack!\n", log_file);
      hexprint(buf, len);
      make_empty_packet(response, rl);
      break;

  }
}
 
int32_t start_daemon(pid_t sid) {
  if (dict_init()) {
    log_format(10, log_file, "Could not initialize the dictionary at path %s!\n", DICTPATH);
    return 1;
  }

  log_format(0, log_file, "Initialized the dictionary with %u entries!\n", dictLen);

  run = 1;

  signal(SIGINT, interrupt_handler);
  signal(SIGHUP, interrupt_handler);

  int32_t ssock, csock, rc;
  socklen_t len;
  int32_t bytesRec = 0;
  struct sockaddr_un ssockaddr = {0};
  struct sockaddr_un csockaddr = {0};

  char buf[1024] = {0};
  ssock = socket(AF_UNIX, SOCK_STREAM, 0);
  if (ssock == -1) {
    log_format(10, log_file, "Socket error: %m!\n");
    return 1;
  }
  len = sizeof(ssockaddr);

  ssockaddr.sun_family = AF_UNIX;
  strcpy(ssockaddr.sun_path, SOCK_PATH);

  unlink(SOCK_PATH);
  rc = bind(ssock, (struct sockaddr *) &ssockaddr, len);
  if (rc == -1) {
    log_format(10, log_file, "Bind error: %m!\n");
    close(ssock);
    return 1;
  }

  rc = listen(ssock, 10);
  if (rc == -1) {
    log_format(10, log_file, "Listen error: %m!\n");
    close(ssock);
    return 1;
  }
  log_string(0, "LISTENING\n", log_file);

  while (run) {
    csock = accept(ssock, (struct sockaddr *) &csockaddr, &len);
    if (csock == -1) {
      log_format(5, log_file, "Accept error: %m!\n");
      close(csock);
      continue;
    }

    len = sizeof(csockaddr);
    rc = getpeername(csock, (struct sockaddr *) &csockaddr, &len);
    if (rc == -1) {
      log_format(5, log_file, "Getpeername error: %m!\n");
      close(csock);
      continue;
    } 
    log_format(0, log_file, "Client socket filepath: %s\n", csockaddr.sun_path);

    char response[512] = {0};
    uint32_t rl = 0;

    while (1) {
      bytesRec = recv(csock, buf, sizeof(buf), 0);
      log_format(0, log_file, "Got bytes: %u\n", bytesRec);
      if (bytesRec == 0) {
        break;
      }
      if (bytesRec == -1) {
        log_format(5, log_file, "Recv error: %m\n");
        close(csock);
        break;
      }
      buf[bytesRec] = '\0';
      interpret_data(buf, bytesRec, response, &rl);

      log_format(0, log_file, "%hhu %hhu %hhu [%s]\n", response[0], response[1], response[2], response + 3);

      rc = send(csock, response, rl, 0);
      if (rc == -1) {
        log_format(5, log_file, "SEND ERROR: %m\n");
        close(csock);
        break;
      }
    }
  }

  if (ssock) {
    close(ssock);
  }

  unlink(SOCK_PATH);
  dict_destroy();
  return recv_sig;
}

int main() {
  pid_t pid, sid;
  setlocale(LC_ALL, "");
  set_logging_level(10);

  pid = fork();
  if (pid < 0) {
    exit(EXIT_FAILURE);
  }
  if (pid > 0) {
    exit(EXIT_SUCCESS);
  }

  umask(0);

  log_file = fopen("/var/log/r2k.log", "a+");
  if (log_file == NULL) {
    log_string(10, "Could not open the log file, aborting!\n", stderr);
    perror("r2k");
  }

  setbuf(log_file, NULL);
  sid = setsid();
  if (sid < 0) {
    log_string(10, "Could not create a new session id, aborting!\n", stderr);
    exit(EXIT_FAILURE);
  }

  if ((chdir("/")) < 0) {
    log_string(10, "Could not change directory to \"/\", aborting!\n", stderr);
    exit(EXIT_FAILURE);
  }

  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);

  int32_t exit_code;

  exit_code = start_daemon(sid);

  log_format(exit_code, log_file, "Exited with code %i\n", exit_code);
  fclose(log_file);
  return exit_code;
}
