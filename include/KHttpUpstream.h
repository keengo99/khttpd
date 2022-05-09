#ifndef KHTTPUPSTREAM_H
#define KHTTPUPSTREAM_H
#include "KTcpUpstream.h"
#include "kbuf.h"
#include "KDechunkEngine.h"
class KDechunkContext
{
public:
	void start_read(ks_buffer* buffer)
	{
		hot = buffer->buf;
		len = buffer->used;
	}
	void save_point(ks_buffer* buffer)
	{
		ks_save_point(buffer, hot, len);
	}
	KDechunkResult dechunk(char** piece, int& piece_length)
	{
		return engine.dechunk(&hot, len, piece, piece_length);
	}
	KDechunkEngine2 engine;
	int len;
	char* hot;
};
struct KUpstreamContext
{
	krw_buffer* send_header_buffer;
	ks_buffer* read_buffer;
	KDechunkContext* dechunk_ctx;
	int64_t left;
};
class KHttpUpstream : public KTcpUpstream
{
public:
	KHttpUpstream(kconnection* cn) : KTcpUpstream(cn)
	{
		memset(&ctx, 0, sizeof(ctx));
	}
	~KHttpUpstream()
	{
		assert(ctx.send_header_buffer == NULL);
		assert(ctx.read_buffer == NULL);
		assert(ctx.dechunk_ctx == NULL);
	}
	int64_t get_left() override
	{
		return ctx.left;
	}
	bool send_connection(const char* val, hlen_t val_len) override;
	bool send_method_path(uint16_t meth, const char* path, hlen_t path_len) override;
	bool send_header(const char* attr, hlen_t attr_len, const char* val, hlen_t val_len) override;
	bool send_host(const char* host, hlen_t host_len) override;
	void set_content_length(int64_t content_length) override;
	int read(char* buf, int len) override;
	KGL_RESULT send_header_complete() override;
	KGL_RESULT read_header() override;
	void clean() override;
private:
	KUpstreamContext ctx;
};
#endif

