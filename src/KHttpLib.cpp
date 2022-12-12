/*
 * Copyright (c) 2010, NanChang BangTeng Inc
 * All Rights Reserved.
 *
 * You may use the Software for free for non-commercial use
 * under the License Restrictions.
 *
 * You may modify the source code(if being provieded) or interface
 * of the Software under the License Restrictions.
 *
 * You may use the Software for commercial use after purchasing the
 * commercial license.Moreover, according to the license you purchased
 * you may get specified term, manner and content of technical
 * support from NanChang BangTeng Inc
 *
 * See COPYING file for detail.
 */
#include "kfeature.h"
#ifdef MALLOCDEBUG
#include 	<map>
#endif
#ifndef 	_WIN32
#include	<syslog.h>
#include <unistd.h>
#endif
#include	<time.h>
#include 	<ctype.h>
#include <string>
#include <sstream>
#include	"kforwin32.h"
#include "kmalloc.h"
#include "KHttpLib.h"
#include "KUrl.h"
#include "KHttpHeader.h"

kgl_str_t kgl_header_type_string[] = {
	{ kgl_expand_string("Unknow") },
	{ kgl_expand_string("Internal") },
	{ kgl_expand_string("Server") },
	{ kgl_expand_string("Date") },
	{ kgl_expand_string("Content-Length")},
	{ kgl_expand_string("Last-Modified")},
};
static const char* b64alpha = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
#define B64PAD '='

static const char* days[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
static const char* months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun","Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
static int make_month(const char* s);
static int make_num(const char* s);
static int timz_minutes = 0;
void init_time_zone()
{
	time_t tt = time(NULL);
	struct tm t;
#if defined(HAVE_GMTOFF)
	localtime_r(&tt, &t);
	timz_minutes = (int)(t.tm_gmtoff / 60);
#else
	struct tm gmt;
	int days, hours;
	gmtime_r(&tt, &gmt);
	localtime_r(&tt, &t);
	days = t.tm_yday - gmt.tm_yday;
	hours = ((days < -1 ? 24 : 1 < days ? -24 : days * 24) + t.tm_hour - gmt.tm_hour);
	timz_minutes = hours * 60 + t.tm_min - gmt.tm_min;
#endif
}
void CTIME_R(time_t* a, char* b, size_t l)
{
#if	defined(HAVE_CTIME_R)
#if	defined(CTIME_R_3)
	ctime_r(a, b, l);
#else
	ctime_r(a, b);
#endif /* SOLARIS */
#else
	struct tm tm;
	memset(b, 0, l);
	localtime_r(a, &tm);
	snprintf(b, l - 1, "%s %02d %s %02d:%02d:%02d\n", days[tm.tm_wday],
		tm.tm_mday, months[tm.tm_mon], tm.tm_hour, tm.tm_min, tm.tm_sec);
#endif /* HAVE_CTIME_R */
}

void makeLastModifiedTime(time_t* a, char* b, size_t l) {
	struct tm tm;
	memset(b, 0, l);
	localtime_r(a, &tm);
	snprintf(b, l - 1, "%02d-%s-%04d %02d:%02d", tm.tm_mday, months[tm.tm_mon],
		1900 + tm.tm_year, tm.tm_hour, tm.tm_min);

}
void do_exit(int code) {
	//   flush_log();
	assert(0);
	//  exit(code);
}

static int make_num(const char* s) {
	if (*s >= '0' && *s <= '9')
		return 10 * (*s - '0') + *(s + 1) - '0';
	else
		return *(s + 1) - '0';
}

static int make_month(const char* s) {
	int i;
	for (i = 0; i < 12; i++) {
		if (!strncasecmp(months[i], s, 3))
			return i;
	}
	return -1;
}

static bool isRightTime(struct tm* tm) {
	if (tm->tm_sec < 0 || tm->tm_sec > 59)
		return false;
	if (tm->tm_min < 0 || tm->tm_min > 59)
		return false;
	if (tm->tm_hour < 0 || tm->tm_hour > 23)
		return false;
	if (tm->tm_mday < 1 || tm->tm_mday > 31)
		return false;
	if (tm->tm_mon < 0 || tm->tm_mon > 11)
		return false;
	return true;
}

bool parse_date_elements(const char* day, const char* month, const char* year,
	const char* aTime, const char* zone, struct tm* tm) {
	const char* t;
	memset(tm, 0, sizeof(struct tm));

	if (!day || !month || !year || !aTime)
		return false;
	tm->tm_mday = atoi(day);
	tm->tm_mon = make_month(month);
	if (tm->tm_mon < 0)
		return false;
	tm->tm_year = atoi(year);
	if (strlen(year) == 4)
		tm->tm_year -= 1900;
	else if (tm->tm_year < 70)
		tm->tm_year += 100;
	else if (tm->tm_year > 19000)
		tm->tm_year -= 19000;
	tm->tm_hour = make_num(aTime);
	t = strchr(aTime, ':');
	if (!t)
		return false;
	t++;
	tm->tm_min = atoi(t);
	t = strchr(t, ':');
	if (t)
		tm->tm_sec = atoi(t + 1);
	return isRightTime(tm);
}

bool parse_date(const char* str, struct tm* tm) {
	char* day = NULL;
	char* month = NULL;
	char* year = NULL;
	char* aTime = NULL;
	char* zone = NULL;
	char* buf = xstrdup(str);
	char* hot = buf;
	for (;;) {
		hot = strchr(hot, ' ');
		if (hot == NULL) {
			break;
		}
		*hot = 0;
		hot += 1;
		if (!day) {
			day = hot;
		} else if (!month) {
			month = hot;
		} else if (!year) {
			year = hot;
		} else if (!aTime) {
			aTime = hot;
		} else {
			zone = hot;
			break;
		}
	}
	bool result = parse_date_elements(day, month, year, aTime, zone, tm);
	xfree(buf);
	return result;
}

time_t parse1123time(const char* str) {
	struct tm tm;
	time_t t;
	if (NULL == str)
		return -1;
	if (!parse_date(str, &tm)) {
		return -1;
	}
	tm.tm_isdst = -1;
#ifdef HAVE_TIMEGM
	t = timegm(&tm);
#elif HAVE_GMTOFF
	t = mktime(&tm);
	if (t != -1) {
		struct tm local;
		localtime_r(&t, &local);
		t += local.tm_gmtoff;
	}
#else
	t = mktime(&tm);
	if (t != -1) {
		time_t dst = 0;
#if defined (_TIMEZONE)
#elif defined (_timezone)
#elif defined(AIX)
#elif defined(CYGWIN)
#elif defined(MSWIN)
#else
		extern long timezone;
#endif
		if (tm.tm_isdst > 0)
			dst = -3600;
#if defined ( _timezone) || defined(_WIN32)
		t -= (_timezone + dst);
#else
		t -= (timezone + dst);
#endif
	}
#endif
	return t;
}
int make_http_time(time_t time, char* buf, int size)
{
	struct tm tm;
	time_t holder = time;
	gmtime_r(&holder, &tm);
	return snprintf(buf, size, "%s, %02d %s %d %02d:%02d:%02d GMT", days[tm.tm_wday],
		tm.tm_mday, months[tm.tm_mon], tm.tm_year + 1900, tm.tm_hour,
		tm.tm_min, tm.tm_sec);
}
const char* mk1123time(time_t time, char* buf, int size) {
	make_http_time(time, buf, size);
	return buf;
}
void my_msleep(int msec) {
#if defined(_WIN32)
	Sleep(msec);
#else
	/* DU don't want to sleep in poll when number of descriptors is 0 */
	usleep(msec * 1000);

#endif
	/*
	struct timeval tv;
	tv.tv_sec = msec / 1000;
	tv.tv_usec = (msec % 1000) * 1000;
	select(1, NULL, NULL, NULL, &tv);
	*/
}
#define	BU_FREE	1
#define	BU_BUSY	2
const char* log_request_time(time_t time, char* buf, size_t buf_size) {
	int timz = timz_minutes;
	struct tm t;
	localtime_r(&time, &t);
	char sign = (timz < 0 ? '-' : '+');
	if (timz < 0) {
		timz = -timz;
	}
	snprintf(buf, buf_size - 1, "[%02d/%s/%d:%02d:%02d:%02d %c%.2d%.2d]",
		t.tm_mday, months[t.tm_mon], t.tm_year + 1900, t.tm_hour, t.tm_min,
		t.tm_sec, sign, timz / 60, timz % 60);
	return buf;
}

static unsigned char hexchars[] = "0123456789ABCDEF";
char* url_value_encode(const char* s, size_t len, size_t* new_length) {
	unsigned char c;
	unsigned char* to, * start;
	unsigned char const* from, * end;
	if (len == 0) {
		//assert(false);
		return strdup("");
	}
	from = (unsigned char*)s;
	end = (unsigned char*)s + len;
	start = to = (unsigned char*)xmalloc(3 * len + 1);

	while (from < end) {
		c = *from++;
		if (c == '/') {
			*to++ = c;
		} else if (!isascii((int)c)) {
			to[0] = '%';
			to[1] = hexchars[c >> 4];
			to[2] = hexchars[c & 15];
			to += 3;
		} else {
			*to++ = c;
		}
	}
	*to = 0;
	if (new_length) {
		*new_length = to - start;
	}
	return (char*)start;
}
int kgl_atoi(const u_char* line, size_t n)
{
	int  value;
	if (n == 0) {
		return 0;
	}
	for (value = 0; n--; line++) {
		if (*line < '0' || *line > '9') {
			return value;
		}	
		value = value * 10 + (*line - '0');
	}
	return value;
}
void kgl_strlow(u_char* dst, u_char* src, size_t n)
{
	while (n) {
		*dst = kgl_tolower(*src);
		dst++;
		src++;
		n--;
	}
}


const char* kgl_memstr(const char* haystack, size_t haystacklen, const char* needle, size_t needlen)
{
	const char* p;
	for (p = (char *)haystack; p <= (haystack - needlen + haystacklen); p++) {
		if (memcmp(p, needle, needlen) == 0)
			return p; /* found */
	}
	return NULL;
}
char* url_encode(const char* s, size_t len, size_t* new_length) {
	unsigned char c;
	unsigned char* to, * start;
	unsigned char const* from, * end;
	if (len == 0) {
		//assert(false);
		return strdup("");
	}
	from = (unsigned char*)s;
	end = (unsigned char*)s + len;
	start = to = (unsigned char*)xmalloc(3 * len + 1);

	while (from < end) {
		c = *from++;
		if (c == '/') {
			*to++ = c;
			/*
			} else if (c == ' ') {
				*to++ = '+';
			*/
		} else if ((c < '0' && c != '-' && c != '.') || (c < 'A' && c > '9')
			|| (c > 'Z' && c < 'a' && c != '_') || (c > 'z') || c == '\\') {
			to[0] = '%';
			to[1] = hexchars[c >> 4];
			to[2] = hexchars[c & 15];
			to += 3;
		} else {
			*to++ = c;
		}
	}
	*to = 0;
	if (new_length) {
		*new_length = to - start;
	}
	return (char*)start;
}
std::string url_encode(const char* str, size_t len_string) {
	std::string s;
	if (len_string == 0) {
		len_string = strlen(str);
	}
	char* new_string = url_encode(str, len_string, NULL);
	if (new_string) {
		s = new_string;
		xfree(new_string);
	}
	return s;
}

bool parse_url(const char* src, size_t len, KUrl* url) {
	const char* host, * path;
	if (len == 0) {
		return false;
	}
	size_t p_len;
	if (*src == '/') {
		path = src;
		goto only_path;
	}
	host = kgl_memstr(src, len, kgl_expand_string("://"));	
	if (!host) {
		return false;
	}
	p_len = (host - src);
	if (p_len == 4 && strncasecmp(src, "http", p_len) == 0) {
		KBIT_CLR(url->flags, KGL_URL_ORIG_SSL);
		//url->port = 80;
	} else if (p_len == 5 && strncasecmp(src, "https", p_len) == 0) {
		KBIT_SET(url->flags, KGL_URL_ORIG_SSL);
		//url->port = 443;
	}
	
	//host start
	host += 3;
	len -= (p_len + 3);
	path = (char*)memchr(host, '/', len);
	if (path == NULL) {
		return false;
	}
	size_t host_len = path - host;
	len -= host_len;
	if (!url->parse_host(host, host_len)) {
		return false;
	}
only_path: const char* sp = (char *)memchr(path, '?', len);	
	size_t path_len;
	if (sp) {
		path_len = sp - path;
		sp++;
		len--;
		char* param = kgl_strndup(sp,len);
		assert(url->param == NULL);
		if (*param) {
			url->param = param;
		} else {
			free(param);
		}
	} else {
		path_len = len;
	}
	assert(url->path == NULL);
	url->path = kgl_strndup(path, path_len);
	return true;
}
bool parse_url(const char* src, KUrl* url) {
	return parse_url(src, strlen(src), url);
}


static int my_htoi(char* s) {
	int value;
	int c;

	c = ((unsigned char*)s)[0];
	if (isupper(c))
		c = tolower(c);
	value = (c >= '0' && c <= '9' ? c - '0' : c - 'a' + 10) * 16;

	c = ((unsigned char*)s)[1];
	if (isupper(c))
		c = tolower(c);
	value += c >= '0' && c <= '9' ? c - '0' : c - 'a' + 10;

	return (value);
}
int url_decode(char* str, int len, KUrl* url, bool space2plus)
{
	char* dest = str;
	char* data = str;
	bool mem_availble = false;
	if (len == 0) {
		len = (int)strlen(str);
	}
	while (len--) {
		if (space2plus && *data == '+') {
			*dest = ' ';
			if (url) {
				KBIT_SET(url->flags, KGL_URL_ENCODE);
			}
		} else if (
			*data == '%' &&
			len >= 2 &&
			isxdigit((unsigned char)*(data + 1)) &&
			isxdigit((unsigned char)*(data + 2))) {
			mem_availble = true;
			*dest = (char)my_htoi(data + 1);
			data += 2;
			len -= 2;
			if (url) {
				KBIT_SET(url->flags, KGL_URL_ENCODE);
			}
		} else {
			*dest = *data;
		}
		data++;
		dest++;
	}
	if (mem_availble) {
		*dest = '\0';
	}
	return (int)(dest - str);
}




unsigned int str_chr(const char* s, int c) {
	char ch;
	const char* t;

	ch = c;
	t = s;
	for (;;) {
		if (!*t)
			break;
		if (*t == ch)
			break;
		++t;
		if (!*t)
			break;
		if (*t == ch)
			break;
		++t;
		if (!*t)
			break;
		if (*t == ch)
			break;
		++t;
		if (!*t)
			break;
		if (*t == ch)
			break;
		++t;
	}
	return (unsigned)(t - s);
}

char* b64decode(const unsigned char* in, int* l) {
	int i, j;
	//	int len;
	unsigned char a[4];
	unsigned char b[3];
	char* s;
	if (*l <= 0) {
		*l = (int)strlen((const char*)in);
	}
	char* out = (char*)xmalloc(2 * (*l) + 2);
	if (out == NULL) {
		return NULL;
	}
	s = out;
	for (i = 0; i < *l; i += 4) {
		for (j = 0; j < 4; j++)
			if ((i + j) < *l && in[i + j] != B64PAD) {
				a[j] = str_chr(b64alpha, in[i + j]);
				if (a[j] > 63) {
					//		printf("bad char=%c,j=%d\n",a[j],j);
					free(out);
					return NULL;
					//	return -1;
				}
			} else {
				a[j] = 0;
			}

		b[0] = (a[0] << 2) | (a[1] >> 4);
		b[1] = (a[1] << 4) | (a[2] >> 2);
		b[2] = (a[2] << 6) | (a[3]);

		*s = b[0];
		s++;

		if (in[i + 1] == B64PAD)
			break;
		*s = b[1];
		s++;

		if (in[i + 2] == B64PAD)
			break;
		*s = b[2];
		s++;
	}

	*l = (int)(s - out);
	//  printf("len=%d\n",len);
	while (*l && !out[*l - 1])
		--* l;
	out[*l] = 0;
	//	string result = out;
	//	free(out);
	return out;
	// return len;

	//  return s.str();
}

std::string b64encode(const unsigned char* in, int len)
/* not null terminated */
{
	unsigned char a, b, c;
	int i;
	// char *s;
	std::stringstream s;
	if (len == 0) {
		len = (int)strlen((const char*)in);
	}

	// if (!stralloc_ready(out,in->len / 3 * 4 + 4)) return -1;
	// s = out->s;
	for (i = 0; i < len; i += 3) {
		a = in[i];
		b = i + 1 < len ? in[i + 1] : 0;
		c = i + 2 < len ? in[i + 2] : 0;

		s << b64alpha[a >> 2];
		s << b64alpha[((a & 3) << 4) | (b >> 4)];

		if (i + 1 >= len)
			s << B64PAD;
		else
			s << b64alpha[((b & 15) << 2) | (c >> 6)];

		if (i + 2 >= len)
			s << B64PAD;
		else
			s << b64alpha[c & 63];
	}
	return s.str();
}
