#ifndef KHTTP2SINK_H_99
#define KHTTP2SINK_H_99 1
#include "KTcpServerSink.h"
#include "KHttp2.h"
#ifdef ENABLE_HTTP2
class KHttp2Sink : public KTcpServerSink
{
public:
	KHttp2Sink(KHttp2 *http2,KHttp2Context *ctx,kgl_pool_t *pool): KTcpServerSink(pool)
	{
		this->http2 = http2;
		this->ctx = ctx;
		this->data.raw_url = new KUrl;
	}
	~KHttp2Sink()
	{
		kassert(ctx == NULL);
	}
	bool set_transfer_chunked() override
	{
		return false;
	}
	bool internal_response_status(uint16_t status_code) override
	{
		return http2->add_status(ctx, status_code);
	}
	bool response_header(const char *name, int name_len, const char *val, int val_len) override
	{
		return http2->add_header(ctx, name, name_len,val, val_len);
	}
	bool response_connection(const char *val, int val_len) override
	{
		return false;
	}
	//返回头长度,-1表示出错
	int internal_start_response_body(int64_t body_size) override
	{
		ctx->SetContentLength(body_size);
		return http2->send_header(ctx);
	}
	bool is_locked() override
	{
		if (ctx->read_wait) {
			return true;
		}
		if (ctx->write_wait && !IS_WRITE_WAIT_FOR_HUP(ctx->write_wait)) {
			return true;
		}
		return false;
	}
	bool read_hup(void *arg, result_callback result) override
	{
		http2->read_hup(ctx, result, arg);
		return true;
	}
	void remove_read_hup() override
	{
		http2->remove_read_hup(ctx);
	}
	int internal_read(char *buf, int len) override
	{
		WSABUF bufs;
		bufs.iov_base = buf;
		bufs.iov_len = len;
		return http2->read(ctx, &bufs, 1);
	}
	int internal_write(WSABUF *buf, int bc) override
	{
		return http2->write(ctx, buf, bc);
	}

	int end_request() override
	{
		KBIT_SET(data.flags, RQ_CONNECTION_CLOSE);
		if (unlikely(KBIT_TEST(data.flags, RQ_BODY_NOT_COMPLETE))) {
			http2->shutdown(ctx);
		} else {
			http2->write_end(ctx);
		}
		http2->release(ctx);
#ifndef NDEBUG
		ctx = NULL;
#endif
		delete this;
		return 0;
	}
	void shutdown() override
	{
		return http2->shutdown(ctx);
	}
	kconnection *get_connection() override
	{
		return http2->c;
	}
	void set_time_out(int tmo_count) override
	{
		ctx->tmo = tmo_count;
		ctx->tmo_left = tmo_count;
	}
	int get_time_out() override
	{
		return ctx->tmo;
	}
	void flush() override
	{
	}
#ifdef ENABLE_PROXY_PROTOCOL
	const char *GetProxyIp()
	{
		return NULL;
	};
#endif
protected:
	friend class KHttp2;
private:
	KHttp2Context *ctx;
	KHttp2 *http2;
};
#endif
#endif
