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

#ifndef KSTRING_H_
#define KSTRING_H_
#include <string>
#include <stdlib.h>
#include <assert.h>
#include <exception>
#include "KStream.h"
#include "kmalloc.h"
class KStringBuf;
class KString final
{
public:
	KString(KString& a) {
		if (a.s.len > 0) {
			assign(a.s.data, a.s.len);
			return;
		}
		s = { 0 };
	}
	KString(KString&& s) noexcept {
		this->s = s.s;
		s.s = { 0 };
	}
	KString(KStringBuf&& a) noexcept;
	KString() : s{ 0 } {
	}
	~KString() {
		if (s.data) {
			free(s.data);
		}
	}
	size_t size() const {
		return s.len;
	}
	const char* c_str() const {
		if (!s.data) {
			return "";
		}
		return s.data;
	}
	const kgl_str_t& str() const {
		return s;
	}
	KString& operator = (KString&& a) noexcept {
		if (this == &a) {
			return *this;
		}
		if (this->s.data) {
			free(this->s.data);
		}
		this->s.data = a.s.data;
		this->s.len = a.s.len;
		a.s = { 0 };
		return *this;
	}
	KString& operator = (const KString& a) {
		if (this == &a) {
			//×Ô¸³Öµ
			return *this;
		}
		assign(a.s.data, a.s.len);
		return *this;
	}

	friend class KStringBuf;
private:
	void assign(const char* str, size_t len) {
		char* buf = (char*)malloc(len + 1);
		if (!buf) {
			throw std::bad_alloc();
		}
		if (s.data) {
			free(s.data);
		}
		s.data = buf;
		memcpy(s.data, str, len);
		s.data[len] = '\0';
		return;
	}
	kgl_str_t s;
};

class KStringBuf final : public KWStream
{
public:
	KStringBuf(KStringBuf& s) = delete;
	KStringBuf(int size) {
		assert((size & (size - 1)) == 0);
		align_size = size;
		if (align_size <= 0) {
			align_size = 32;

		}
		if (align_size > max_align_size) {
			align_size = max_align_size;
		}
		current_size = 0;
	}
	KStringBuf() : KStringBuf(32) {
	}
	~KStringBuf() {
	}
	void clean() {
		current_size += (int)s.s.len;
		s.s.len = 0;
	}
	const char* c_str() {
		if (!end_with_zero()) {
			return "";
		}
		s.s.data[s.s.len] = '\0';
		return s.s.data;
	}
	const KString& str() const {
		return s;
	}
	char* buf() {
		return s.s.data;
	}
	int size() const {
		return (int)s.s.len;
	}
	char* steal() {
		if (!end_with_zero()) {
			return nullptr;
		}
		auto ret = s.s.data;
		s.s = { 0 };
		current_size = 0;
		return ret;
	}
	KStringBuf& operator = (const KStringBuf& s) = delete;
	KGL_RESULT write_all(const char* str, int len) override {
		if (!append(str, len)) {
			return KGL_ENO_MEMORY;
		}
		return KGL_OK;
	}
	friend class KString;
private:
	bool end_with_zero() {
		if (!guarantee(1)) {
			return false;
		}
		s.s.data[s.s.len] = '\0';
		return true;
	}
	static constexpr int max_align_size = 2048;
	bool append(const char* str, size_t len) {
		if (!guarantee((int)len + 1)) {
			return false;
		}
		kgl_memcpy(s.s.data + s.s.len, str, len);
		s.s.len += (int)len;
		current_size -= (int)len;
		return true;
	}
	bool guarantee(int size) {
		if (current_size >= size) {
			return true;
		}
		int new_size = (int)s.s.len + size;
		new_size = kgl_align(new_size, align_size);
		if (!kgl_realloc((void**)&s.s.data, new_size)) {
			return false;
		}
		current_size = new_size - (int)s.s.len;
		assert(current_size >= size);
		return true;
	}
	int current_size;
	int align_size;
	KString s;
};
class KFixString final : public KRStream
{
public:
	KFixString(const char* buf, size_t len) : s{ (char*)buf,len } {
	};
	int64_t get_left() override {
		return (int64_t)s.len;
	}
	int read(char* buf, int len) override {
		int send_len = (int)(KGL_MIN((size_t)len, s.len));
		if (send_len <= 0) {
			return 0;
		}
		kgl_memcpy(buf, s.data, send_len);
		s.data += send_len;
		s.len -= send_len;
		return send_len;
	}
private:
	kgl_str_t s;
};
inline kgl_str_t operator "" _CS(const char* str, size_t len) {
	return kgl_str_t{ (char*)str, len };
}
#endif /* KSTRING_H_ */
