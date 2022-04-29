#ifndef KHTTP2UPSTREAM_H
#define KHTTP2UPSTREAM_H
#include "KHttp2.h"
#ifdef ENABLE_UPSTREAM_HTTP2
#include "KPoolableSocketContainer.h"
#include "KUpstream.h"
class KHttp2Upstream : public KUpstream
{
public:
	KHttp2Upstream(KHttp2 *http2, KHttp2Context *ctx)
	{
		this->http2 = http2;
		this->ctx = ctx;
		pool = NULL;
	}
	~KHttp2Upstream()
	{
		kassert(pool == NULL);
		kassert(http2 == NULL);
		kassert(ctx == NULL);
	}
	kconnection *GetConnection()
	{
		return http2->c;
	}
	void WriteEnd()
	{
		http2->write_end(ctx);
	}
	void SetTimeOut(int tmo)
	{
		ctx->tmo = tmo;
		ctx->tmo_left = tmo;
	}
	void BindOpaque(KOPAQUE data)
	{
		ctx->data = data;
	}
	KOPAQUE GetOpaque()
	{
		return ctx->data;
	}
	int Read(char* buf, int len)
	{
		WSABUF bufs;
		bufs.iov_base = buf;
		bufs.iov_len = len;
		return http2->read(ctx, &bufs, 1);
	}
	int Write(WSABUF* buf, int bc)
	{
		return http2->write(ctx, buf, bc);
	}
	bool send_header(const char* attr, hlen_t attr_len, const char* val, hlen_t val_len);
	bool send_method_path(uint16_t meth, const char* path, hlen_t path_len);
	bool send_host(const char* host, hlen_t host_len);
	bool send_content_length(int64_t content_length);
	bool send_header_complete(int64_t post_body_len);

	KGL_RESULT read_header();
	bool set_header_callback(void *arg, kgl_header_callback header);
	KUpstream *NewStream();
	kgl_pool_t *GetPool()
	{
		if (pool == NULL) {
			pool = kgl_create_pool(8192);
		}
		return pool;
	}
	void Shutdown()
	{
		http2->shutdown(ctx);
	}
	void Destroy()
	{
		kassert(http2);
		kassert(ctx);
		http2->release(ctx);
		ctx = NULL;
		http2 = NULL;
		delete this;
	}
	bool IsMultiStream()
	{
		return true;
	}

	sockaddr_i *GetAddr()
	{
		return &http2->c->addr;
	}
	void gc(int life_time,time_t last_recv_time)
	{
		life_time = 30;
		if (pool) {
			kgl_destroy_pool(pool);
			pool = NULL;
		}
		if (container == NULL) {
			Destroy();
			return;
		}
		if (ctx->admin_stream) {
			http2->release_stream(ctx);
			container->gcSocket(this, life_time, last_recv_time);
			return;
		}
		KHttp2Upstream *admin_stream = http2->get_admin_stream();
		if (admin_stream) {
			container->bind(admin_stream);
			container->gcSocket(admin_stream, life_time, last_recv_time);
		}
		Destroy();
	}
	friend class KHttp2Env;
private:
	KHttp2 *http2;
	KHttp2Context *ctx;
	kgl_pool_t *pool;
};
#endif
#endif
