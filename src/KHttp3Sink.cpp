#include "KHttp3Sink.h"
#include "KHttpServer.h"
#include "KPreRequest.h"
#ifdef ENABLE_HTTP3
void KHttp3Sink::start(int got) {
	void* hset = lsquic_stream_get_hset(st);
	if (hset == NULL) {
		return;
	}
	struct header_decoder* req = (struct header_decoder*)hset;
	free_header_decoder(req);
	khttp_server_new_request(this, 0);
}
void KHttp3Sink::on_read(lsquic_stream_t* st) {
	lsquic_stream_wantread(st, 0);
	if (this->st == NULL) {
		assert(!is_processing());
		assert(!KBIT_TEST(st_flags, STF_READ));
		this->st = st;
		KBIT_SET(st_flags, STF_RREADY | H3_IS_PROCESSING);
		kfiber_create(kgl_sink_start_fiber, (KSink*)this, 0, http_config.fiber_stack_size, NULL);
		return;
	}
	assert(KBIT_TEST(st_flags, STF_READ));
	ev[OP_READ].result = (int)lsquic_stream_readv(st, ev[OP_READ].buf, ev[OP_READ].bc);
	KBIT_CLR(st_flags, STF_RREADY | STF_READ);
	ev[OP_READ].cd->f->notice(ev[OP_READ].cd, ev[OP_READ].result);
}
void KHttp3Sink::on_write(lsquic_stream_t* st) {
	lsquic_stream_wantwrite(st, 0);
	assert(KBIT_TEST(st_flags, STF_WRITE));
	if (!KBIT_TEST(st_flags, STF_WRITE)) {
		return;
	}
	KBIT_CLR(st_flags, STF_WRITE);
	if (!KBIT_TEST(st_flags, H3_HEADER_SENT)) {
		assert(!KBIT_TEST(st_flags, STF_WRITE));
		KBIT_SET(st_flags, H3_HEADER_SENT);
		lsquic_http_headers_t headers;
		headers.headers = response_headers.begin();;
		headers.count = response_headers.get_size();
		//printf("write stream header count=[%d] st=[%p]\n", headers.count,st);
		ev[OP_WRITE].result = lsquic_stream_send_headers(st, &headers, 0);
	} else {
		ev[OP_WRITE].result = (int)lsquic_stream_writev(st, ev[OP_WRITE].buf, ev[OP_WRITE].bc);
	}
	//printf("write result=[%d],st=[%p]\n", ev[OP_WRITE].result,st);
	ev[OP_WRITE].cd->f->notice(ev[OP_WRITE].cd, ev[OP_WRITE].result);
}
#endif
