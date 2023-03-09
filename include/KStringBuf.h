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
#include "KHttpLib.h"
class KStringBuf;


class KString final
{
public:
	KString(const KString& a) : KString(kstring_refs(a.s)) {
	}
	KString(KStringBuf&& s) noexcept;
	KString(KString&& a) noexcept {
		this->s = a.s;
		a.s = nullptr;
	}
	KString() : s{ 0 } {
	}
	KString(const char* str) {
		s = kstring_from(str);
	}
	KString(const std::string& a) : s{ 0 } {
		assign(a.c_str(), a.size());
	}
	KString(kgl_ref_str_t* a) {
		s = a;
	}
	explicit operator bool() const noexcept {
		if (s && s->data) {
			return true;
		}
		return false;
	}
	KString(const kgl_ref_str_t& a) : s{ 0 } {
		if (a.len > 0) {
			assign(a.data, a.len);
		}
	}
	~KString() {
		kstring_release(s);
	}
	bool empty() const {
		return size() == 0;
	}
	uint32_t size() const {
		if (s) {
			return s->len;
		}
		return 0;
	}
	const char* c_str() const {
		if (s) {
			return s->data;
		}
		return "";
	}
	const kgl_ref_str_t& str() const {
		return *s;
	}
	kgl_ref_str_t* data() {
		return s;
	}
	KString& operator = (KString&& a) noexcept {
		if (this == &a) {
			return *this;
		}
		kstring_release(s);
		s = a.s;
		a.s = nullptr;
		return *this;
	}
	KString& operator = (const KString& a) {
		if (this == &a) {
			//×Ô¸³Öµ
			return *this;
		}
		kstring_release(s);
		s = kstring_refs(a.s);
		return *this;
	}
	KString& operator = (const char* a) {
		assign(a, (uint32_t)strlen(a));
		return *this;
	}
	bool operator< (const KString& a) const {
		if (s && a.s) {
			return kgl_cmp(s->data, s->len, a.s->data, a.s->len) < 0;
		}
		return s == nullptr;
	}
	friend class KStringBuf;
private:
	void assign(const char* str, size_t len) {
		kstring_release(s);
		s = kstring_from2(str, len);
	}
	kgl_ref_str_t* s;
};

class KStringBuf final : public KWStream
{
public:
	KStringBuf(KStringBuf& s) = delete;
	KStringBuf(uint32_t size) {
		s = convert_refs_string(nullptr, 0);
		if (!s) {
			throw std::bad_alloc();
		}
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
		kstring_release(s);
	}
	void clean() {
		current_size += s->len;
		s->len = 0;
	}
	const char* c_str() {
		if (!end_with_zero()) {
			return "";
		}
		return s->data;
	}
	KString str() {
		end_with_zero();
		return KString(kstring_refs(s));
	}
	KString reset() {
		end_with_zero();
		auto ns = convert_refs_string(NULL, 0);
		if (!ns) {
			return KString();
		}
		auto s = this->s;
		this->s = ns;
		current_size = 0;
		return KString(s);
	}
	char* buf() const {
		return s->data;
	}
	int size() const {
		return (int)s->len;
	}
	kgl_auto_cstr steal() {
		if (!end_with_zero()) {
			return nullptr;
		}
		auto ret = s->data;
		s->data = nullptr;
		s->len = 0;
		current_size = 0;
		return kgl_auto_cstr(ret);
	}
	KStringBuf& operator = (const KStringBuf& s) = delete;
	KGL_RESULT write_all(const char* str, int len) override {
		if (!append(str, len)) {
			return KGL_ENO_MEMORY;
		}
		return KGL_OK;
	}
	friend class KString;
	bool end_with_zero() {
		if ((current_size > 16 || current_size == 0) && !realloc(s->len + 1)) {
			return false;
		}
		s->data[s->len] = '\0';
		return true;
	}
private:
	static constexpr int max_align_size = 2048;
	bool append(const char* str, size_t len) {
		if (!guarantee((uint32_t)len + 1)) {
			return false;
		}
		kgl_memcpy(s->data + s->len, str, len);
		s->len += (uint32_t)len;
		current_size -= (uint32_t)len;
		return true;
	}
	bool guarantee(uint32_t size) {
		if (current_size >= size) {
			return true;
		}
		return realloc(s->len + size);
	}
	bool realloc(uint32_t new_size) {
		assert(new_size > s->len);
		assert(s);
		if (!kgl_realloc((void**)&s->data, new_size)) {
			return false;
		}
		current_size = new_size - s->len;
		return true;
	}
	uint32_t current_size;
	uint32_t align_size;
	kgl_ref_str_t* s;
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
inline kgl_ref_str_t operator "" _CS(const char* str, size_t len) {
	return kgl_ref_str_t{ (char*)str, (uint32_t)len, 1 };
}
#endif /* KSTRING_H_ */
