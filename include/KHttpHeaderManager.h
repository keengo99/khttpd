#ifndef KHTTPHEADERMANAGER_H_99
#define KHTTPHEADERMANAGER_H_99
#include <ctype.h>
#include "KHttpHeader.h"
#include "KHttpLib.h"

#if 0
inline int attr_casecmp(const char* s1, const char* s2)
{
	const unsigned char* p1 = (const unsigned char*)s1;
	const unsigned char* p2 = (const unsigned char*)s2;
	int result;
	if (p1 == p2)
		return 0;

	while ((result = kgl_attr_tolower(*p1) - kgl_attr_tolower(*p2++)) == 0)
		if (*p1++ == '\0')
			break;
	return result;
}
#endif
enum class KHttpHeaderIteratorResult
{
	Continue,
	Remove,
	Free
};

typedef KHttpHeaderIteratorResult(*kgl_http_header_iterator) (void* arg, KHttpHeader* av);

class KHttpHeaderManager
{
public:
	void append(KHttpHeader* new_t)
	{
		if (header == NULL) {
			header = last = new_t;
			return;
		}
		new_t->next = header;
		header = new_t;
	}
	void insert(KHttpHeader* new_t)
	{
		if (header == NULL) {
			header = last = new_t;
			return;
		}
		kassert(last);
		last->next = new_t;
		last = new_t;
		return;
	}
	void iterator(kgl_http_header_iterator it, void* arg)
	{
		KHttpHeader* l = header;
		last = NULL;
	next:
		while (l) {
			KHttpHeader* next = l->next;
			switch (it(arg, l)) {
			case KHttpHeaderIteratorResult::Remove:
				if (last) {
					last->next = next;
				} else {
					header = next;
				}
				l = next;
				goto next;
			case KHttpHeaderIteratorResult::Free:
			{
				if (last) {
					last->next = next;
				} else {
					header = next;
				}
				xfree_header(l);
				l = next;
				goto next;
			}
			default:
				break;
			}
			last = l;
			l = next;
		}
		assert((header == NULL && last == NULL) || (header != NULL && last->next == NULL));
	}
	KHttpHeader *add_header(const char* attr, int attr_len, const char* val, int val_len, bool tail = true)
	{
		KHttpHeader* new_t = new_http_header(attr, attr_len, val, val_len);
		if (new_t == NULL) {
			return nullptr;
		}
		if (tail) {
			insert(new_t);
			return new_t;
		}
		append(new_t);
		return new_t;
	}
	bool add_header(kgl_header_type type, time_t tm)
	{
		char tmp_buf[42];
		mk1123time(tm, tmp_buf, 41);
		return add_header(type, tmp_buf, 29);
	}
	KHttpHeader *add_header(kgl_header_type type, const char* val, int val_len, bool tail = true)
	{
		KHttpHeader* new_t = new_http_know_header(type, val, val_len);
		if (new_t == NULL) {
			return nullptr;
		}
		if (tail) {
			insert(new_t);
			return new_t;
		}
		append(new_t);
		return new_t;
	}
	KHttpHeader* find(kgl_header_type attr) {
		KHttpHeader* l = header;
		while (l) {
			if (l->name_is_know && l->know_header == attr) {
				return l;
			}
			l = l->next;
		}
		return NULL;
	}
	KHttpHeader* find(const char* attr, int len)
	{
		KHttpHeader* l = header;
		while (l) {			
			if (kgl_is_attr(l, attr, len)) {
				return l;
			}
			l = l->next;
		}
		return NULL;
	}
	KHttpHeader* remove(const char* attr,int attr_len)
	{
		KHttpHeader* l = header;
		last = NULL;
		while (l) {
			if (kgl_is_attr(l, attr, attr_len)) {
				if (last) {
					last->next = l->next;
				} else {
					header = l->next;
				}
				return l;
			}
			last = l;
			l = l->next;
		}
		assert((header == NULL && last == NULL) || (header != NULL && last->next == NULL));
		return NULL;
	}
	KHttpHeader* get_header()
	{
		return header;
	}
	KHttpHeader* steal_header()
	{
		KHttpHeader* h = header;
		header = last = NULL;
		return h;
	}
	KHttpHeader* header;
	KHttpHeader* last;
};
#endif
