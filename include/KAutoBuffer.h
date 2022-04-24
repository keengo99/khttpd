#ifndef KAUTOBUFFER_H
#define KAUTOBUFFER_H
#include "kbuf.h"
#include "khttp.h"
#include "kmalloc.h"

class KAutoBufferData
{
public:
	kbuf *head;
	kbuf *last;
	char *hot;
	int total_len;
	kgl_pool_t *pool;
};
class KAutoBuffer :public KAutoBufferData
{
public:
	KAutoBuffer(kgl_pool_t *pool)
	{
		memset(static_cast<KAutoBufferData *>(this), 0, sizeof(KAutoBufferData));
		this->pool = pool;
	}
	KAutoBuffer()
	{
		memset(static_cast<KAutoBufferData *>(this), 0, sizeof(KAutoBufferData));
	}
	~KAutoBuffer()
	{
		clean();
	}
	void clean()
	{
		if (this->pool == NULL) {
			destroy_kbuf(head);
			memset(static_cast<KAutoBufferData *>(this), 0, sizeof(KAutoBufferData));
		}
	}
	int write(const char* buf, int len);
	void writeSuccess(int got);
	char *getWriteBuffer(int &len);
	void Insert(const char *str, int len) {
		kbuf *buf = new_buff(len);
		kgl_memcpy(buf->data, str, len);
		buf->used = len;
		Insert(buf);
	}
	inline void Insert(kbuf *buf)
	{
		buf->next = head;
		if (last == NULL) {
			last = buf;
		}
		head = buf;
		total_len += buf->used;
	}
	inline void Append(kbuf *buf)
	{
		if (last == NULL) {
			kassert(head == NULL);
			head = buf;
		} else {
			assert(head);
			last->next = buf;
		}
		buf->next = NULL;
		hot = NULL;
		last = buf;
		total_len += buf->used;
	}
	void SwitchRead()
	{
		if (head) {
			hot = head->data;
		}
	}
	inline void print()
	{
		kbuf *tmp = head;
		while (tmp) {
			if (tmp->used > 0) {
				fwrite(tmp->data, 1, tmp->used, stdout);
			}
			tmp = tmp->next;
		}
	}
	unsigned getLen()
	{
		return total_len;
	}
	kbuf *getHead()
	{
		return head;
	}
	kbuf *stealBuffFast()
	{
		kbuf *ret = head;
		head = last = NULL;
		hot = NULL;
		return ret;
	}
	kbuf *stealBuff();
	int getReadBuffer(LPWSABUF buffer, int bufferCount);
	bool readSuccess(int *got);
	inline kbuf *new_buff(int chunk_size)
	{
		if (pool == NULL) {
			kbuf *buf = (kbuf *)xmalloc(sizeof(kbuf));
			buf->flags = 0;
			buf->data = (char *)xmalloc(chunk_size);
			buf->used = 0;
			buf->next = NULL;
			return buf;
		}
		kbuf *buf = (kbuf *)kgl_pnalloc(pool, sizeof(kbuf));
		buf->flags = 0;
		buf->data = (char *)kgl_pnalloc(pool, chunk_size);
		buf->used = 0;
		buf->next = NULL;
		return buf;
	}
};
#endif

