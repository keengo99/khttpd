/*
 * Copyright (c) 2010, NanChang BangTeng Inc
 *
 * kangle web server              http://www.kangleweb.net/
 * ---------------------------------------------------------------------
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.
 *  See COPYING file for detail.
 *
 *  Author: KangHongjiu <keengo99@gmail.com>
 */
#ifndef KSTREAM_H_
#define KSTREAM_H_
#include <time.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <stdlib.h>
#include "kforwin32.h"
#include "kstring.h"
#include "khttp.h"
#include "kbuf.h"

#define KBuffedWStream KWStream
#define WSTR(x) write_all(x,sizeof(x)-1)
#define WSTRING(x) write_all(NULL, x,sizeof(x)-1)

#define INT2STRING_LEN	32
inline int int2string2(int64_t value, char* buf, bool hex = false) {
	const char* formatString = INT64_FORMAT;
	if (hex) {
		formatString = INT64_FORMAT_HEX;
	}
	int size = snprintf(buf, INT2STRING_LEN - 1, formatString, value);
	buf[size] = '\0';
	return size;
}
inline const char* int2string(int64_t value, char* buf, bool hex = false) {
	int2string2(value, buf, hex);
	return buf;
}

inline char* kgl_upstrndup(const char* val, size_t val_len) {
	size_t len;
	char* copy;
	len = strnlen(val, val_len);
	copy = (char*)malloc(len + 1);
	if (copy) {
		char* end = copy + len;
		char* dst = copy;
		while (dst < end) {
			*dst = kgl_toupper(*val);
			val++;
			dst++;
		}
		*dst = '\0';
	}
	return copy;
}
inline char* upstrdup(const char* val) {
	char* buf = strdup(val);
	char* v = buf;
	while (*v) {
		*v = toupper(*v);
		v++;
	}
	return buf;
}
#define StreamState KGL_RESULT
#define STREAM_WRITE_FAILED  KGL_EUNKNOW
#define STREAM_WRITE_SUCCESS KGL_OK
#define STREAM_WRITE_END     KGL_END

class KRStream
{
public:
	virtual ~KRStream() {
	}
	virtual int64_t get_left() = 0;
	virtual int read(char* buf, int len) = 0;
	virtual void release() {
		delete this;
	}
	bool read_all(char* buf, int len);
};
class KWStream
{
public:
	virtual ~KWStream() {

	}
	KWStream() {

	}
	virtual KGL_RESULT write_all(WSABUF* bufs, int bc);
	virtual KGL_RESULT write_all(const char* buf, int len);
	virtual KGL_RESULT flush() {
		return KGL_OK;
	}
	virtual KGL_RESULT write_end(KGL_RESULT result) {
		return flush();
	}
	virtual void release() {
		delete this;
	}
	KGL_RESULT write_all(const char* buf);
	inline KWStream& operator <<(const char* str) {
		if (KGL_OK != write_all(str, (int)strlen(str))) {
			fprintf(stderr, "cann't write to stream 1\n");
		}
		return *this;
	}
	inline bool add(const int c, const char* fmt) {
		char buf[16];
		int len = snprintf(buf, sizeof(buf) - 1, fmt, c);
		if (len > 0) {
			return write_all(buf, len) == STREAM_WRITE_SUCCESS;
		}
		return false;
	}
	inline bool add(const INT64 c, const char* fmt) {
		char buf[INT2STRING_LEN];
		int len = snprintf(buf, sizeof(buf) - 1, fmt, c);
		if (len > 0) {
			return write_all(buf, len) == STREAM_WRITE_SUCCESS;
		}
		return false;
	}
	inline KWStream& operator <<(const std::string str) {
		write_all(str.c_str(), (int)str.size());
		return *this;
	}
	inline KWStream& operator <<(const char c) {
		write_all(&c, 1);
		return *this;
	}
	inline KWStream& operator <<(const int value) {
		char buf[16];
		int len = snprintf(buf, 15, "%d", value);
		if (len <= 0) {
			return *this;
		}
		if (KGL_OK != write_all(buf, len)) {
			fprintf(stderr, "cann't write to stream 2\n");
		}
		return *this;
	}
	inline KWStream& operator <<(const INT64 value) {
		char buf[INT2STRING_LEN];
		int2string(value, buf, false);
		if (KGL_OK != write_all(buf, (int)strlen(buf))) {
			fprintf(stderr, "cann't write to stream 3\n");
		}
		return *this;
	}
	inline bool add_as_hex(const int value) {
		return add(value, "%x");
	}
	inline KWStream& operator <<(const unsigned value) {
		char buf[16];
		const char* fmt = "%u";
		int len = snprintf(buf, sizeof(buf) - 1, fmt, value);
		if (len <= 0) {
			return *this;
		}
		if (KGL_OK != write_all(buf, len)) {
			fprintf(stderr, "cann't write to stream 4\n");
		}
		return *this;
	}
protected:
	virtual int write(const char* buf, int len) {
		return -1;
	}
};
class KConsole : public KWStream
{
public:
	int write(const char* buf, int len);
	static KConsole out;
};
class KStream : public KRStream, public KWStream
{
};

#endif /* KSTREAM_H_ */
