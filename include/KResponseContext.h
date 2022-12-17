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
	KResponseContext(kgl_pool_t *pool) : ab(pool)
	{
	}
	void head_insert_const(const char *str,uint16_t len)
	{
		kbuf *t = (kbuf *)kgl_pnalloc(ab.pool, sizeof(kbuf));
		t->data = (char *)str;
		t->used = len;
		ab.Insert(t);
	}
	void head_append(char *str,uint16_t len)
	{
		kbuf *t = (kbuf *)kgl_pnalloc(ab.pool,sizeof(kbuf));
		memset(t,0,sizeof(kbuf));
		t->data = str;
		t->used = len;
		ab.Append(t);
	}
	void head_append_const(const char *str,uint16_t len)
	{
		kbuf *t = (kbuf *)kgl_pnalloc(ab.pool, sizeof(kbuf));
		t->data = (char *)str;
		t->used = len;
		ab.Append(t);
	}
	friend class KHttpSink;
private:
	KAutoBuffer ab;
};
#endif
