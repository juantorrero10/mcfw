/* C code produced by gperf version 3.1 */
/* Command-line: gperf -t -L C -S 1 ext_lookup.gperf  */
/* Computed positions: -k'3' */

#if !((' ' == 32) && ('!' == 33) && ('"' == 34) && ('#' == 35) \
      && ('%' == 37) && ('&' == 38) && ('\'' == 39) && ('(' == 40) \
      && (')' == 41) && ('*' == 42) && ('+' == 43) && (',' == 44) \
      && ('-' == 45) && ('.' == 46) && ('/' == 47) && ('0' == 48) \
      && ('1' == 49) && ('2' == 50) && ('3' == 51) && ('4' == 52) \
      && ('5' == 53) && ('6' == 54) && ('7' == 55) && ('8' == 56) \
      && ('9' == 57) && (':' == 58) && (';' == 59) && ('<' == 60) \
      && ('=' == 61) && ('>' == 62) && ('?' == 63) && ('A' == 65) \
      && ('B' == 66) && ('C' == 67) && ('D' == 68) && ('E' == 69) \
      && ('F' == 70) && ('G' == 71) && ('H' == 72) && ('I' == 73) \
      && ('J' == 74) && ('K' == 75) && ('L' == 76) && ('M' == 77) \
      && ('N' == 78) && ('O' == 79) && ('P' == 80) && ('Q' == 81) \
      && ('R' == 82) && ('S' == 83) && ('T' == 84) && ('U' == 85) \
      && ('V' == 86) && ('W' == 87) && ('X' == 88) && ('Y' == 89) \
      && ('Z' == 90) && ('[' == 91) && ('\\' == 92) && (']' == 93) \
      && ('^' == 94) && ('_' == 95) && ('a' == 97) && ('b' == 98) \
      && ('c' == 99) && ('d' == 100) && ('e' == 101) && ('f' == 102) \
      && ('g' == 103) && ('h' == 104) && ('i' == 105) && ('j' == 106) \
      && ('k' == 107) && ('l' == 108) && ('m' == 109) && ('n' == 110) \
      && ('o' == 111) && ('p' == 112) && ('q' == 113) && ('r' == 114) \
      && ('s' == 115) && ('t' == 116) && ('u' == 117) && ('v' == 118) \
      && ('w' == 119) && ('x' == 120) && ('y' == 121) && ('z' == 122) \
      && ('{' == 123) && ('|' == 124) && ('}' == 125) && ('~' == 126))
/* The character set is not based on ISO-646.  */
error "gperf generated tables don't work with this execution character set. Please report a bug to <bug-gperf@gnu.org>."
#endif

#line 1 "ext_lookup.gperf"

#include "../include/public/http.h"
struct mime_ext_map { const char *name; int id; };
#line 5 "ext_lookup.gperf"
struct mime_ext_map;
/* maximum key range = 16, duplicates = 0 */

#ifdef __GNUC__
__inline
#else
#ifdef __cplusplus
inline
#endif
#endif
static unsigned int
ext_hash (str, len)
     register const char *str;
     register size_t len;
{
  static const unsigned char asso_values[] =
    {
      20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
      20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
      20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
      20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
      20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
      20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
      20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
      20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
      20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
      20, 20, 20, 20, 20, 20, 20, 20, 20,  3,
      14, 20, 20, 20,  5,  9, 20, 20, 20, 20,
      15,  0, 10, 20, 20,  5,  0, 20, 10, 20,
       0, 20, 20, 20, 20, 20, 20, 20
    };
  return len + asso_values[(unsigned char)str[2]];
}

const struct mime_ext_map *
mime_ext_lookup (str, len)
     register const char *str;
     register size_t len;
{
  enum
    {
      TOTAL_KEYWORDS = 13,
      MIN_WORD_LENGTH = 3,
      MAX_WORD_LENGTH = 6,
      MIN_HASH_VALUE = 4,
      MAX_HASH_VALUE = 19
    };

  static const struct mime_ext_map ext_wordlist[] =
    {
#line 17 "ext_lookup.gperf"
      {".txt", CONTENT_TEXT},
#line 18 "ext_lookup.gperf"
      {".html", CONTENT_HTML},
#line 29 "ext_lookup.gperf"
      {".woff2", CONTENT_WOFF2},
#line 28 "ext_lookup.gperf"
      {".ico", CONTENT_ICO},
#line 20 "ext_lookup.gperf"
      {".js", CONTENT_JAVASCRIPT},
#line 19 "ext_lookup.gperf"
      {".css", CONTENT_CSS},
#line 21 "ext_lookup.gperf"
      {".json", CONTENT_JSON},
#line 22 "ext_lookup.gperf"
      {".xhtml", CONTENT_XHTML},
#line 23 "ext_lookup.gperf"
      {".bin", CONTENT_BINARY},
#line 25 "ext_lookup.gperf"
      {".svg", CONTENT_SVG},
#line 27 "ext_lookup.gperf"
      {".jpeg", CONTENT_JPEG},
#line 24 "ext_lookup.gperf"
      {".pdf", CONTENT_PDF},
#line 26 "ext_lookup.gperf"
      {".png", CONTENT_PNG}
    };

  if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)
    {
      register unsigned int key = ext_hash (str, len);

      if (key <= MAX_HASH_VALUE && key >= MIN_HASH_VALUE)
        {
          register const struct mime_ext_map *resword;

          switch (key - 4)
            {
              case 0:
                if (len == 4)
                  {
                    resword = &ext_wordlist[0];
                    goto compare;
                  }
                break;
              case 1:
                if (len == 5)
                  {
                    resword = &ext_wordlist[1];
                    goto compare;
                  }
                break;
              case 2:
                if (len == 6)
                  {
                    resword = &ext_wordlist[2];
                    goto compare;
                  }
                break;
              case 3:
                if (len == 4)
                  {
                    resword = &ext_wordlist[3];
                    goto compare;
                  }
                break;
              case 4:
                if (len == 3)
                  {
                    resword = &ext_wordlist[4];
                    goto compare;
                  }
                break;
              case 5:
                if (len == 4)
                  {
                    resword = &ext_wordlist[5];
                    goto compare;
                  }
                break;
              case 6:
                if (len == 5)
                  {
                    resword = &ext_wordlist[6];
                    goto compare;
                  }
                break;
              case 7:
                if (len == 6)
                  {
                    resword = &ext_wordlist[7];
                    goto compare;
                  }
                break;
              case 9:
                if (len == 4)
                  {
                    resword = &ext_wordlist[8];
                    goto compare;
                  }
                break;
              case 10:
                if (len == 4)
                  {
                    resword = &ext_wordlist[9];
                    goto compare;
                  }
                break;
              case 11:
                if (len == 5)
                  {
                    resword = &ext_wordlist[10];
                    goto compare;
                  }
                break;
              case 14:
                if (len == 4)
                  {
                    resword = &ext_wordlist[11];
                    goto compare;
                  }
                break;
              case 15:
                if (len == 4)
                  {
                    resword = &ext_wordlist[12];
                    goto compare;
                  }
                break;
            }
          return 0;
        compare:
          {
            register const char *s = resword->name;

            if (*str == *s && !memcmp (str + 1, s + 1, len - 1))
              return resword;
          }
        }
    }
  return 0;
}
/* C code produced by gperf version 3.1 */
/* Command-line: gperf -t -L C -S 1 str_lookup.gperf  */
/* Computed positions: -k'$' */

#if !((' ' == 32) && ('!' == 33) && ('"' == 34) && ('#' == 35) \
      && ('%' == 37) && ('&' == 38) && ('\'' == 39) && ('(' == 40) \
      && (')' == 41) && ('*' == 42) && ('+' == 43) && (',' == 44) \
      && ('-' == 45) && ('.' == 46) && ('/' == 47) && ('0' == 48) \
      && ('1' == 49) && ('2' == 50) && ('3' == 51) && ('4' == 52) \
      && ('5' == 53) && ('6' == 54) && ('7' == 55) && ('8' == 56) \
      && ('9' == 57) && (':' == 58) && (';' == 59) && ('<' == 60) \
      && ('=' == 61) && ('>' == 62) && ('?' == 63) && ('A' == 65) \
      && ('B' == 66) && ('C' == 67) && ('D' == 68) && ('E' == 69) \
      && ('F' == 70) && ('G' == 71) && ('H' == 72) && ('I' == 73) \
      && ('J' == 74) && ('K' == 75) && ('L' == 76) && ('M' == 77) \
      && ('N' == 78) && ('O' == 79) && ('P' == 80) && ('Q' == 81) \
      && ('R' == 82) && ('S' == 83) && ('T' == 84) && ('U' == 85) \
      && ('V' == 86) && ('W' == 87) && ('X' == 88) && ('Y' == 89) \
      && ('Z' == 90) && ('[' == 91) && ('\\' == 92) && (']' == 93) \
      && ('^' == 94) && ('_' == 95) && ('a' == 97) && ('b' == 98) \
      && ('c' == 99) && ('d' == 100) && ('e' == 101) && ('f' == 102) \
      && ('g' == 103) && ('h' == 104) && ('i' == 105) && ('j' == 106) \
      && ('k' == 107) && ('l' == 108) && ('m' == 109) && ('n' == 110) \
      && ('o' == 111) && ('p' == 112) && ('q' == 113) && ('r' == 114) \
      && ('s' == 115) && ('t' == 116) && ('u' == 117) && ('v' == 118) \
      && ('w' == 119) && ('x' == 120) && ('y' == 121) && ('z' == 122) \
      && ('{' == 123) && ('|' == 124) && ('}' == 125) && ('~' == 126))
/* The character set is not based on ISO-646.  */
error "gperf generated tables don't work with this execution character set. Please report a bug to <bug-gperf@gnu.org>."
#endif

#line 1 "str_lookup.gperf"

#include "../include/public/http.h"
struct mime_str_map { const char *name; int id; };
#line 5 "str_lookup.gperf"
struct mime_str_map;
/* maximum key range = 28, duplicates = 0 */

#ifdef __GNUC__
__inline
#else
#ifdef __cplusplus
inline
#endif
#endif
static unsigned int
str_hash (str, len)
     register const char *str;
     register size_t len;
{
  static const unsigned char asso_values[] =
    {
      36, 36, 36, 36, 36, 36, 36, 36, 36, 36,
      36, 36, 36, 36, 36, 36, 36, 36, 36, 36,
      36, 36, 36, 36, 36, 36, 36, 36, 36, 36,
      36, 36, 36, 36, 36, 36, 36, 36, 36, 36,
      36, 36, 36, 36, 36, 36, 36, 36, 36, 36,
      25, 36, 36, 36, 36, 36, 36, 36, 36, 36,
      36, 36, 36, 36, 36, 36, 36, 36, 36, 36,
      36, 36, 36, 36, 36, 36, 36, 36, 36, 36,
      36, 36, 36, 36, 36, 36, 36, 36, 36, 36,
      36, 36, 36, 36, 36, 36, 36, 36, 36, 36,
      36, 36, 15, 10, 36, 36, 36, 36,  0, 10,
       0, 36, 36, 36, 36,  0, 10, 36, 36, 36,
      36, 36, 36, 36, 36, 36, 36, 36
    };
  return len + asso_values[(unsigned char)str[len - 1]];
}

const struct mime_str_map *
mime_str_lookup (str, len)
     register const char *str;
     register size_t len;
{
  enum
    {
      TOTAL_KEYWORDS = 16,
      MIN_WORD_LENGTH = 8,
      MAX_WORD_LENGTH = 24,
      MIN_HASH_VALUE = 8,
      MAX_HASH_VALUE = 35
    };

  static const struct mime_str_map str_wordlist[] =
    {
#line 21 "str_lookup.gperf"
      {"text/css", CONTENT_CSS},
#line 18 "str_lookup.gperf"
      {"text/html", CONTENT_HTML},
#line 17 "str_lookup.gperf"
      {"text/plain", CONTENT_TEXT},
#line 28 "str_lookup.gperf"
      {"image/svg+xml", CONTENT_SVG},
#line 23 "str_lookup.gperf"
      {"application/xml", CONTENT_XML},
#line 22 "str_lookup.gperf"
      {"application/json", CONTENT_JSON},
#line 29 "str_lookup.gperf"
      {"image/png", CONTENT_PNG},
#line 30 "str_lookup.gperf"
      {"image/jpeg", CONTENT_JPEG},
#line 26 "str_lookup.gperf"
      {"application/xhtml+xml", CONTENT_XHTML},
#line 31 "str_lookup.gperf"
      {"image/vnd.microsoft.icon", CONTENT_ICO},
#line 19 "str_lookup.gperf"
      {"text/javascript", CONTENT_JAVASCRIPT},
#line 24 "str_lookup.gperf"
      {"application/wasm", CONTENT_WASM},
#line 20 "str_lookup.gperf"
      {"text/event-stream", CONTENT_EVENT_STREAM},
#line 27 "str_lookup.gperf"
      {"application/pdf", CONTENT_PDF},
#line 25 "str_lookup.gperf"
      {"application/octet-stream", CONTENT_BINARY},
#line 32 "str_lookup.gperf"
      {"font/woff2", CONTENT_WOFF2}
    };

  if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)
    {
      register unsigned int key = str_hash (str, len);

      if (key <= MAX_HASH_VALUE && key >= MIN_HASH_VALUE)
        {
          register const struct mime_str_map *resword;

          switch (key - 8)
            {
              case 0:
                if (len == 8)
                  {
                    resword = &str_wordlist[0];
                    goto compare;
                  }
                break;
              case 1:
                if (len == 9)
                  {
                    resword = &str_wordlist[1];
                    goto compare;
                  }
                break;
              case 2:
                if (len == 10)
                  {
                    resword = &str_wordlist[2];
                    goto compare;
                  }
                break;
              case 5:
                if (len == 13)
                  {
                    resword = &str_wordlist[3];
                    goto compare;
                  }
                break;
              case 7:
                if (len == 15)
                  {
                    resword = &str_wordlist[4];
                    goto compare;
                  }
                break;
              case 8:
                if (len == 16)
                  {
                    resword = &str_wordlist[5];
                    goto compare;
                  }
                break;
              case 11:
                if (len == 9)
                  {
                    resword = &str_wordlist[6];
                    goto compare;
                  }
                break;
              case 12:
                if (len == 10)
                  {
                    resword = &str_wordlist[7];
                    goto compare;
                  }
                break;
              case 13:
                if (len == 21)
                  {
                    resword = &str_wordlist[8];
                    goto compare;
                  }
                break;
              case 16:
                if (len == 24)
                  {
                    resword = &str_wordlist[9];
                    goto compare;
                  }
                break;
              case 17:
                if (len == 15)
                  {
                    resword = &str_wordlist[10];
                    goto compare;
                  }
                break;
              case 18:
                if (len == 16)
                  {
                    resword = &str_wordlist[11];
                    goto compare;
                  }
                break;
              case 19:
                if (len == 17)
                  {
                    resword = &str_wordlist[12];
                    goto compare;
                  }
                break;
              case 22:
                if (len == 15)
                  {
                    resword = &str_wordlist[13];
                    goto compare;
                  }
                break;
              case 26:
                if (len == 24)
                  {
                    resword = &str_wordlist[14];
                    goto compare;
                  }
                break;
              case 27:
                if (len == 10)
                  {
                    resword = &str_wordlist[15];
                    goto compare;
                  }
                break;
            }
          return 0;
        compare:
          {
            register const char *s = resword->name;

            if (*str == *s && !memcmp (str + 1, s + 1, len - 1))
              return resword;
          }
        }
    }
  return 0;
}
