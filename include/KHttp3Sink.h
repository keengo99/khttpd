#ifndef KGL_HTTP3_INCLUDED_H_
#define KGL_HTTP3_INCLUDED_H_
#include "KSink.h"
#include "KHttp3.h"
#ifdef ENABLE_HTTP3
#include "lsxpack_header.h"
#include "kfiber_sync.h"
#include "KObjArray.h"
#include "kfiber.h"

#define  H3_STATUS_SENT    (1<<17)
#define  H3_HEADER_SENT    (1<<18)
#define  H3_IS_PROCESSING  (1<<19)
extern "C" {
	void lsquic_stream_maybe_reset(struct lsquic_stream*, uint64_t error_code, int);
}
struct kgl_fiber_event
{
	kfiber_cond* cd;
	WSABUF* buf;
	int bc;
	int result;
};
class KHttp3Sink : public KSink
{
public:
	KHttp3Sink() : KSink(NULL), response_headers(8)
	{
		this->st = NULL;
		this->data.raw_url = new KUrl;
		data.http_major = 3;
		st_flags = 0;
		for (int i = 0; i < 2; i++) {
			ev[i].cd = kfiber_cond_init(true);
		}
		response_headers.new_first();
	}
	~KHttp3Sink()
	{
		for (int i = 0; i < 2; i++) {
			ev[i].cd->f->release(ev[i].cd);
		}
		assert(!is_processing());
		assert(st == NULL);
	}
	void detach_stream()
	{
		assert(is_processing());
		st = NULL;
	}
	bool is_processing() {
		return KBIT_TEST(st_flags, H3_IS_PROCESSING) > 0;
	}
	bool set_transfer_chunked() override
	{
		return false;
	}
	bool internal_response_status(uint16_t status_code) override
	{
		char buf[4];
		snprintf(buf, sizeof(buf), "%03d", status_code);
		auto header = response_headers[0];
		build_header(header,kgl_expand_string(":status"), buf, 3);
		return true;
	}
	bool response_header(const char* name, int name_len, const char* val, int val_len) override
	{
		auto header = response_headers.get_new();
		if (header == NULL) {
			return false;
		}		
		build_header(header,name,name_len,val,val_len);
		return true;
	}
	bool response_connection(const char* val, int val_len) override
	{
		return false;
	}
	//返回头长度,-1表示出错
	int internal_start_response_body(int64_t body_size) override
	{
		if (st == NULL) {
			return -1;
		}
		assert(!KBIT_TEST(st_flags, STF_WRITE));
		lsquic_stream_wantwrite(st, 1);
		ev[OP_WRITE].cd->f->wait(ev[OP_WRITE].cd);
		return ev[OP_WRITE].result;
	}
	bool is_locked() override
	{
		return KBIT_TEST(st_flags,STF_READ|STF_WRITE)>0;
	}
	bool read_hup(void* arg, result_callback result) override
	{
		//http2->read_hup(ctx, result, arg);
		return true;
	}
	void remove_read_hup() override
	{
		//http2->remove_read_hup(ctx);
	}
	int internal_read(char* buf, int len) override
	{
		assert(!KBIT_TEST(st_flags, STF_READ));
		if (st == NULL) {
			return -1;
		}
		WSABUF bufs;
		bufs.iov_base = buf;
		bufs.iov_len = len;
		ev[OP_READ].buf = &bufs;
		ev[OP_READ].bc = 1;
		if (KBIT_TEST(st_flags, STF_RREADY)) {
			on_read(st);
		} else {
			if (lsquic_stream_wantread(st, 1) < 0) {
				return -1;
			}
		}
		KBIT_SET(st_flags, STF_READ);
		ev[OP_READ].cd->f->wait(ev[OP_READ].cd);
		assert(!KBIT_TEST(st_flags, STF_READ));
		return ev[OP_READ].result;
	}
	int internal_write(WSABUF* buf, int bc) override
	{
		assert(!KBIT_TEST(st_flags, STF_WRITE));
		if (st == NULL) {
			return -1;
		}
		ev[OP_WRITE].buf = buf;
		ev[OP_WRITE].bc = bc;
		if (lsquic_stream_wantwrite(st, 1) < 0) {
			return -1;
		}
		KBIT_SET(st_flags, STF_WRITE);
		ev[OP_WRITE].cd->f->wait(ev[OP_WRITE].cd);
		assert(!KBIT_TEST(st_flags, STF_WRITE));
		return ev[OP_WRITE].result;
	}
	void on_read(lsquic_stream_t *st);
	void on_write(lsquic_stream_t* st);
	int end_request() override
	{
		KBIT_SET(data.flags, RQ_CONNECTION_CLOSE);
		if (unlikely(KBIT_TEST(data.flags, RQ_BODY_NOT_COMPLETE)) && st) {
			lsquic_stream_maybe_reset(st, 0, 0);
		}		
		assert(is_processing());
		KBIT_CLR(st_flags, H3_IS_PROCESSING);
		if (st) {
			lsquic_stream_close(st);
			st = NULL;
			return 0;
		}
		//quic stream already closed.
		delete this;
		return 0;
	}
	void shutdown() override
	{
		//lsquic_stream_shutdown(st,SHUT_RDWR);
	}
	kconnection* get_connection() override
	{
		return nullptr;
	}
	void set_time_out(int tmo_count) override
	{
	
	}
	int get_time_out() override
	{
		return 30;
	}
	void flush() override
	{
		if (st) {
			lsquic_stream_flush(st);
		}
	}
	uint32_t get_server_model() override
	{
		return 0;
	}
	KOPAQUE get_server_opaque() override
	{
		return 0;
	}
private:
	void build_header(lsxpack_header* header,const char* name, int name_len, const char* val, int val_len)
	{
		memset(header, 0, sizeof(lsxpack_header));
		size_t buf_len = name_len + val_len + 2;
		header->buf = (char*)kgl_pnalloc(pool, buf_len);
		memset(header->buf, 0, buf_len);
		kgl_strlow((u_char*)header->buf, (u_char*)name, name_len);
		header->name_len = name_len;
		header->val_len = val_len;
		header->val_offset = name_len+1;
		kgl_memcpy(header->buf + header->val_offset, val, val_len);
	}
	KObjArray<lsxpack_header> response_headers;
	uint32_t st_flags;
	lsquic_stream_t* st;
	kgl_fiber_event ev[2];
};
struct header_decoder
{
	bool have_xhdr;
	bool is_first;
	struct lsxpack_header xhdr;
	size_t       decode_off;
	KHttp3Sink* sink;
	char         decode_buf[MIN(LSXPACK_MAX_STRLEN + 1, 64 * 1024)];
};

inline void free_header_decoder(struct header_decoder* r) {
	free(r);
}
#endif
#endif
