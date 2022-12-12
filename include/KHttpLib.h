#if	!defined(_LIB_H_INCLUDED_)
#define _LIB_H_INCLUDED_
#include "kforwin32.h"
#include <string>
#define kgl_tolower(c)      (u_char) ((c >= 'A' && c <= 'Z') ? (c | 0x20) : c)
#define kgl_toupper(c)      (u_char) ((c >= 'a' && c <= 'z') ? (c & ~0x20) : c)
std::string b64encode(const unsigned char* in, int len = 0);
char* b64decode(const unsigned char* in, int* l);
char *url_encode(const char *s, size_t len, size_t *new_length);
char *url_value_encode(const char *s, size_t len, size_t *new_length);
std::string url_encode(const char *str, size_t len_string);

void my_msleep(int msec);
time_t parse1123time(const char *str);
const char *mk1123time(time_t time, char *buf, int size);
void makeLastModifiedTime(time_t *a, char *b, size_t l);
void my_sleep(int);
void init_time_zone();
uint16_t string_hash(const char *str, uint16_t res = 1);
class KUrl;
bool parse_url(const char* src, KUrl* url);
bool parse_url(const char* src, size_t len, KUrl* url);
int kgl_atoi(const u_char* line, size_t n);
const char* kgl_memstr(const char* haystack, size_t haystacklen,const char* needle, size_t needlen);
void kgl_strlow(u_char* dst, u_char* src, size_t n);
int url_decode(char* str, int len, KUrl* url, bool space2plus);
void CTIME_R(time_t* a, char* b, size_t l);
/*
 * 18446744073709551616
 * buf min size length is 22
 */
extern int program_rand_value;
extern int open_file_limit;
#endif	/* !_LIB_H_INCLUDED_ */
