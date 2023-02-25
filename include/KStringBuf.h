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
#include "KStream.h"
#include "kmalloc.h"

class KString final
{
public:
	KString(KString& a) : KString(a.align_size) {
		if (a.s.len > 0) {
			append(a.s.data, (int)a.s.len);
		}
	}
	KString(KString&& s) noexcept {
		align_size = s.align_size;
		current_size = s.current_size;
		this->s = s.s;
		s.s = { 0 };
		s.current_size = 0;
	}
	KString(int size) : s{ 0 }, current_size{ 0 } {
		assert((size& (size - 1)) == 0);
		align_size = size;
		if (align_size <= 0) {
			align_size = 32;
		}
		if (align_size > max_align_size) {
			align_size = max_align_size;
		}
	}
	KString() : KString(32) {

	}
	~KString() {
		if (s.data) {
			free(s.data);
		}
	}
	const char* c_str() {
		if (!guarantee(1)) {
			return nullptr;
		}
		s.data[s.len] = '\0';
		return s.data;
	}
	size_t size() const {
		return s.len;
	}
	char* buf() {
		return s.data;
	}
	KString& operator +=(const kgl_str_t& s) {
		append(s.data, (int)s.len);
		return *this;
	}
	KString& operator << (const kgl_str_t& s) {
		append(s.data, (int)s.len);
		return *this;
	}
	const kgl_str_t& str() const {
		return s;
	}
	void clean() {
		current_size += (int)s.len;
		s.len = 0;
	}
	KString& operator = (KString&& s) noexcept {
		if (this == &s) {
			return *this;
		}
		if (this->s.data) {
			free(this->s.data);
		}
		this->s.data = s.s.data;
		this->s.len = s.s.len;
		this->current_size = s.current_size;
		this->align_size = s.align_size;
		s.s = { 0 };
		return *this;
	}
	KString& operator = (const KString& s) {
		if (this == &s) {
			//×Ô¸³Öµ
			return *this;
		}
		clean();
		append(s.s.data, s.s.len);
		return *this;
	}
	static constexpr int max_align_size = 2048;
	bool append(const char* str, size_t len) {
		if (!guarantee((int)len + 1)) {
			return false;
		}
		kgl_memcpy(s.data + s.len, str, len);
		s.len += (int)len;
		current_size -= (int)len;
		return true;
	}
	friend class KStringBuf;
protected:
	bool guarantee(int size) {
		if (current_size >= size) {
			return true;
		}
		int new_size = (int)s.len + size;
		new_size = kgl_align(new_size, align_size);
		if (!kgl_realloc((void**)&s.data, new_size)) {
			return false;
		}
		current_size = new_size - (int)s.len;
		assert(current_size >= size);
		return true;
	}
	kgl_str_t s;
	int current_size;
	int align_size;
};

class KStringBuf final : public KWStream
{
public:
	KStringBuf(KStringBuf& s) = delete;
	KStringBuf(int align_size) : s(align_size) {
	}
	KStringBuf() {
	}
	~KStringBuf() {
	}
	void clean() {
		s.clean();
	}
	const char* c_str() {
		return s.c_str();
	}
	const kgl_str_t& str() const {
		return s.s;
	}
	char* buf() {
		return s.s.data;
	}
	int size() const {
		return (int)s.s.len;
	}
	[[nodiscard]] char* steal() {
		if (!s.guarantee(1)) {
			return nullptr;
		}
		s.s.data[s.s.len] = '\0';
		auto ret = s.s.data;
		s.s = { 0 };
		s.current_size = 0;
		return ret;
	}
	KStringBuf& operator = (const KStringBuf& s) = delete;
	KGL_RESULT write_all(const char* str, int len) override {
		if (!s.append(str, len)) {
			return KGL_ENO_MEMORY;
		}
		return KGL_OK;
	}
private:
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
inline kgl_str_t operator ""_CS(const char* str, size_t len) {
	return kgl_str_t{ (char*)str, len };
}
#endif /* KSTRING_H_ */
