#include "KHttp3Sink.h"
#include "KHttpServer.h"
#ifdef ENABLE_HTTP3
void KHttp3Sink::on_read(lsquic_stream_t* st) {
	lsquic_stream_wantread(st, 0);
	if (this->st == NULL) {
		assert(!is_processing());
		assert(!KBIT_TEST(st_flags, STF_READ));
		this->st = st;
		void* hset = lsquic_stream_get_hset(st);
		if (hset == NULL) {
			lsquic_stream_close(st);
			return;
		}
		KBIT_SET(st_flags, STF_RREADY | H3_IS_PROCESSING);
		struct header_decoder* req = (struct header_decoder*)hset;
		free_header_decoder(req);
		kfiber_create(khttp_server_new_request, (KSink*)this, 0, http_config.fiber_stack_size, NULL);
		return;
	}
	assert(KBIT_TEST(st_flags, STF_READ));
	ev[OP_READ].result = (int)lsquic_stream_readv(st, ev[OP_READ].buf, ev[OP_READ].bc);
	KBIT_CLR(st_flags, STF_RREADY | STF_READ);
	ev[OP_READ].cd->f->notice(ev[OP_READ].cd, 0);
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
	ev[OP_WRITE].cd->f->notice(ev[OP_WRITE].cd, 0);
}
#endif
