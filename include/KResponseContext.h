#ifndef KRESPONSECONTEXT_H
#define KRESPONSECONTEXT_H
#include <assert.h>
#ifdef ENABLE_ATOM
#include "katom.h"
#endif
#ifndef _WIN32
#include <sys/uio.h>
#endif
#include "kselector.h"
#include "KAutoBuffer.h"

class KResponseContext
{
public:
	KResponseContext()
	{
		clean();
	}
	void head_insert_const(kgl_pool_t *pool, const char* str, uint16_t len) {
		kbuf* t = (kbuf*)kgl_pnalloc(pool, sizeof(kbuf));
		t->data = (char*)str;
		t->used = len;
		insert(t);
	}
	void head_append(kgl_pool_t* pool, const char* str, uint16_t len) {
		kbuf* t = (kbuf*)kgl_pnalloc(pool, sizeof(kbuf));
		memset(t, 0, sizeof(kbuf));
		t->data = (char *)str;
		t->used = len;
		append(t);
	}	
	inline void clean() {
		head = last = nullptr;
		total_len = 0;
	}
	inline bool empty() {
		return head == nullptr;
	}
	inline void attach(const kbuf* buf, int len) {
		assert(last);
		last->next = (kbuf*)buf;
		total_len += len;
	}
	inline const kbuf* get_buf() {
		return head;
	}
	inline int get_len() {
		return total_len;
	}
	friend class KHttpSink;
private:
	inline void insert(kbuf* buf) {
		buf->next = head;
		if (last == NULL) {
			last = buf;
		}
		head = buf;
		total_len += buf->used;
	}
	inline void append(kbuf* buf) {
		if (last == NULL) {
			kassert(head == NULL);
			head = buf;
		} else {
			assert(head);
			last->next = buf;
		}
		buf->next = NULL;
		last = buf;
		total_len += buf->used;
	}
	kbuf* head;
	kbuf* last;
	int total_len;
};
#endif
