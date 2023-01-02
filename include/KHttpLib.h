#if	!defined(_LIB_H_INCLUDED_)
#define _LIB_H_INCLUDED_
#include "kforwin32.h"
#include <string>
#include <cstring>
#include "khttp.h"
#include "KHttpHeader.h"


std::string b64encode(const unsigned char* in, int len = 0);
char* b64decode(const unsigned char* in, int* l);
char *url_encode(const char *s, size_t len, size_t *new_length);
char *url_value_encode(const char *s, size_t len, size_t *new_length);
std::string url_encode(const char *str, size_t len_string);

void my_msleep(int msec);
u_char* kgl_slprintf(u_char* buf, u_char* last, const char* fmt, ...);
u_char* kgl_snprintf(u_char* buf, size_t max, const char* fmt, ...);
u_char* kgl_sprintf(u_char* buf, const char* fmt, ...);
time_t kgl_parse_http_time(u_char *str,size_t len);
const char *mk1123time(time_t time, char *buf, int size);
void make_last_modified_time(time_t *a, char *b, size_t l);
void init_time_zone();
class KUrl;
bool parse_url(const char* src, KUrl* url);
bool parse_url(const char* src, size_t len, KUrl* url);
int64_t kgl_atol(const u_char* line, size_t n);
int kgl_atoi(const u_char* line, size_t n);
INLINE int64_t string2int(const char* buf) {
	return kgl_atol((u_char *)buf, strlen(buf));
}
#define kgl_memcmp memcmp
int kgl_casecmp(const char* s1, const char* s2, size_t attr_len);
const char* kgl_memstr(const char* haystack, size_t haystacklen,const char* needle, size_t needlen);
void kgl_strlow(u_char* dst, u_char* src, size_t n);

inline bool kgl_mem_same(const char* attr, size_t attr_len, const char* val, size_t val_len)
{
	if (attr_len != val_len) {
		return false;
	}
	return kgl_memcmp(attr, val, attr_len) == 0;
}
inline bool kgl_mem_case_same(const char* s1, size_t attr_len, const char* s2, size_t val_len)
{
	if (attr_len != val_len) {
		return false;
	}
	return kgl_casecmp(s1, s2, val_len) == 0;
}
int kgl_ncasecmp(const char* s1, size_t n1, const char* s2, size_t n2);
int kgl_ncmp(const char* s1, size_t n1, const char* s2, size_t n2);
/**
 * @kgl_mempbrk
 * @brief Finds a character within a specified input buffer.
 *
 * @param[in] str - A pointer to the beginning of the buffer.
 * @param[in] n - The length of the str.
 * @param[in] control - A pointer to a list of characters to match.
 * @param[in] control_len - The length of the \e control buffer.
 * @return A pointer to the first occurance of one of the accept characters
 *   in the input buffer,
 *   else NULL if not found.
 */
const char* kgl_mempbrk(const char* str, size_t n, const char* control, int control_len);
inline int kgl_attr_tolower(const u_char p) {
	if (p == '-') {
		return '_';
	}
	return kgl_tolower((u_char)p);
}
inline bool kgl_is_attr(const char* s1, size_t s1_len, const char* s2, size_t s2_len)
{
	if (s1_len != s2_len) {
		return false;
	}
	while (s1_len > 0) {
		if (kgl_attr_tolower((u_char)*s1++) != kgl_attr_tolower((u_char)*s2++)) {
			return false;
		}
		s1_len--;
	}
	return true;
}
inline bool kgl_is_attr(KHttpHeader* header, const char* s2, size_t s2_len)
{
	if (header->name_is_know) {
		return kgl_is_attr(kgl_header_type_string[header->know_header].value.data, kgl_header_type_string[header->know_header].value.len, s2, s2_len);
	}
	return kgl_is_attr(header->buf, header->name_len, s2, s2_len);
}
/*
skip space or not space
*/
inline const char* kgl_skip_space(const char* start, const char* end, bool space = true)
{
	while (start < end) {
		if ((isspace((unsigned char)*start) != 0) != space) {
			return start;
		}
		start++;
	}
	return NULL;
}
int url_decode(char* str, int len, KUrl* url, bool space2plus);
inline bool is_absolute_path(const char* str) {
	if (str[0] == '/') {
		return true;
	}
#ifdef _WIN32
	if (str[0] == '\\') {
		return true;
	}
	if (strlen(str) > 1 && str[1] == ':') {
		return true;
	}
#endif
	return false;
}
void CTIME_R(time_t* a, char* b, size_t l);
/*
 * 18446744073709551616
 * buf min size length is 22
 */
extern int program_rand_value;
extern int open_file_limit;
#endif	/* !_LIB_H_INCLUDED_ */
