#ifndef KHTTPUPSTREAM_H
#define KHTTPUPSTREAM_H
#include "KTcpUpstream.h"
#include "kbuf.h"

class KHttpUpstream : public KTcpUpstream
{
public:
	KHttpUpstream(kconnection* cn) : KTcpUpstream(cn)
	{
		send_header_buffer = NULL;
		read_buffer = NULL;
	}
	~KHttpUpstream()
	{
		assert(send_header_buffer == NULL);
		assert(read_buffer == NULL);
	}
	bool send_connection(const char* val, hlen_t val_len);
	bool send_method_path(uint16_t meth, const char* path, hlen_t path_len);
	bool send_header(const char* attr, hlen_t attr_len, const char* val, hlen_t val_len);
	bool send_host(const char* host, hlen_t host_len);
	void set_content_length(int64_t content_length);
	int read(WSABUF* buf, int bc);
	KGL_RESULT send_header_complete();
	KGL_RESULT read_header();
	void clean();
private:
	krw_buffer* send_header_buffer;
	ks_buffer* read_buffer;
};
#endif

