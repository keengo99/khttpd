#include <stdio.h>
#include "KTsUpstream.h"
#include "kselectable.h"
#include "kconnection.h"
#include "kfiber.h"
#include "KHttpServer.h"

struct KTsUpstreamParam {
	KUpstream* us;
	union {
		WSABUF* bufs;
		char* buf;
		KTsUpstream* ts;
	};
	union {
		int bc;
		int len;
		int life_time;
	};
};
void http2_header_callback(KOPAQUE data, void* arg, const char* attr, int attr_len, const char* val, int val_len)
{
	return;
#if 0
		KHttpRequest* rq = (KHttpRequest*)arg;
		KAsyncFetchObject* fo = (KAsyncFetchObject*)data;
		assert(kselector_is_same_thread(rq->sink->GetSelector()));
		fo->PushHeader(rq, attr, attr_len, val, val_len, false);
#endif
}
static bool ts_header_callback(KUpstream *us, void *arg, const char *attr, int attr_len, const char *val, int val_len,bool is_first)
{
	KTsUpstream *ts = (KTsUpstream *)arg;
	kgl_pool_t *pool = ts->us->GetPool();
	KHttpHeader *header = new_pool_http_header(pool, attr, attr_len, val, val_len);
	if (ts->header) {
		ts->last_header->next = header;
		ts->last_header = header;
	} else {
		ts->header = header;
		ts->last_header = header;
	}
	return true;
}
static int ts_send_header_complete(void* arg, int len)
{
	KUpstream* us = (KUpstream*)arg;
	return (int)us->send_header_complete();
}
static int ts_shutdown(void* arg, int len)
{
	KUpstream* us = (KUpstream*)arg;
	us->Shutdown();
	return 0;
}
static int ts_write_end(void* arg, int len)
{
	KUpstream* us = (KUpstream*)arg;
	us->write_end();
	return 0;
}
static int ts_gc(void* arg, int len)
{
	KTsUpstreamParam* param = (KTsUpstreamParam*)arg;
	param->us->gc(param->life_time);
	return 0;
}
static int ts_read(void* arg, int len)
{
	KTsUpstreamParam* param = (KTsUpstreamParam*)arg;
	return param->us->read(param->buf, param->len);
}
static int ts_write(void* arg, int len)
{
	KTsUpstreamParam* param = (KTsUpstreamParam*)arg;
	return param->us->write(param->bufs, param->len);
}
static int ts_read_http_header(void* arg, int len)
{
	return (int)((KUpstream*)arg)->read_header();
}
static kev_result next_ts_destroy(KOPAQUE data, void *arg, int got)
{
	KUpstream *us = (KUpstream *)arg;
	us->Destroy();
	return kev_ok;
}

static int ts_set_http2_parser(void* arg, int len)
{
	KTsUpstreamParam* param = (KTsUpstreamParam*)arg;
	param->us->set_header_callback(param->ts, ts_header_callback);
	return 0;
}
KGL_RESULT KTsUpstream::read_header()
{
	kfiber* fiber = NULL;
	if (kfiber_create2(us->GetSelector(), ts_read_http_header, us, 0, http_config.fiber_stack_size, &fiber) != 0) {
		return KGL_ESYSCALL;
	}
	int ret;
	kfiber_join(fiber, &ret);
	bool is_first = true;
	while (header) {
		stack.header(us, stack.arg, header->attr, header->attr_len, header->val, header->val_len,is_first);
		is_first = false;
		header = header->next;
	}
	return (KGL_RESULT)ret;
}
int KTsUpstream::write(WSABUF* buf, int bc)
{
	KTsUpstreamParam param;
	param.us = us;
	param.bufs = buf;
	param.len = bc;
	assert(buf[0].iov_len > 0);
	kfiber* fiber = NULL;
	if (kfiber_create2(us->GetSelector(), ts_write, &param, 0, 0, &fiber) != 0) {
		return -1;
	}
	int ret;
	kfiber_join(fiber, &ret);
	return ret;
}
int KTsUpstream::read(char* buf, int len)
{
	KTsUpstreamParam param;
	param.us = us;
	param.buf = buf;
	param.len = len;
	kfiber* fiber = NULL;
	if (kfiber_create2(us->GetSelector(), ts_read, &param, 0, 0, &fiber) != 0) {
		return -1;
	}
	int ret;
	kfiber_join(fiber, &ret);
	return ret;
}
void KTsUpstream::write_end()
{
	if (!us->IsMultiStream()) {
		us->write_end();
		return;
	}
	kfiber* fiber = NULL;
	kfiber_create2(us->GetSelector(), ts_write_end, us, 0, http_config.fiber_stack_size, &fiber);
	int ret;
	kfiber_join(fiber, &ret);
}
void KTsUpstream::Shutdown()
{
	if (!us->IsMultiStream()) {
		us->Shutdown();
		return;
	}
	kfiber* fiber = NULL;
	kfiber_create2(us->GetSelector(), ts_shutdown, us, 0, http_config.fiber_stack_size, &fiber);
	int ret;
	kfiber_join(fiber, &ret);
}
KGL_RESULT KTsUpstream::send_header_complete()
{
	if (us->IsMultiStream()) {
		return us->send_header_complete();
	}
	kfiber* fiber = NULL;
	kfiber_create2(us->GetSelector(), ts_send_header_complete, us, 0, http_config.fiber_stack_size, &fiber);
	int ret = KGL_EUNKNOW;
	kfiber_join(fiber, &ret);
	return (KGL_RESULT)ret;
}
void KTsUpstream::Destroy()
{
	fprintf(stderr, "never goto here.\n");
	kassert(false);
	if (us) {
		selectable_next(&us->GetConnection()->st, next_ts_destroy, us, 0);
		us = NULL;
	}
	delete this;
}
void KTsUpstream::gc(int life_time)
{
	KTsUpstreamParam param;
	param.life_time = life_time;
	param.us = us;
	kfiber* fiber = NULL;
	kfiber_create2(us->GetSelector(), ts_gc, &param, 0, http_config.fiber_stack_size, &fiber);
	int ret;
	kfiber_join(fiber, &ret);
	us = NULL;
	delete this;
}

bool KTsUpstream::set_header_callback(void* arg, kgl_header_callback header)
{
	stack.arg = arg;
	stack.header = header;
	KTsUpstreamParam param;
	param.us = us;
	param.ts = this;

	kfiber* fiber = NULL;
	if (kfiber_create2(us->GetSelector(), ts_set_http2_parser, &param, 0, http_config.fiber_stack_size, &fiber) != 0) {
		return false;
	}
	int ret;
	kfiber_join(fiber, &ret);
	return true;
}
