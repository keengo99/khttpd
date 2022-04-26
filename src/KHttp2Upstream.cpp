#include "KHttp2Upstream.h"
#include "KStringBuf.h"
#include "khttp.h"
#ifdef ENABLE_UPSTREAM_HTTP2
bool KHttp2Upstream::set_header_callback(void *arg, kgl_header_callback header)
{
	assert(kselector_is_same_thread(http2->c->st.selector));
	assert(!ctx->in_closed);
	assert(ctx->read_wait == NULL);
	kgl_http2_event *e = new kgl_http2_event;
	e->header_arg = arg;
	e->header = header;
	ctx->read_wait = e;
	return true;
}
KGL_RESULT KHttp2Upstream::read_header()
{
	if (ctx->send_header) {
		http2->send_header(ctx);
	}
	if (0 == http2->ReadHeader(ctx)) {
		return KGL_OK;
	} 
	return KGL_EUNKNOW;
}
KUpstream *KHttp2Upstream::NewStream()
{
	kassert(this->ctx->admin_stream);
	return http2->connect();
}
bool KHttp2Upstream::send_header(const char* attr, hlen_t attr_len, const char* val, hlen_t val_len)
{
	return http2->add_header(ctx, attr, attr_len, val, val_len);
}
bool KHttp2Upstream::send_method_path(uint16_t meth, const char* path, hlen_t path_len)
{
	if (!http2->add_method(ctx, meth)) {
		return false;
	}
	return http2->add_header(ctx, kgl_expand_string(":path"), path, path_len);
}
bool KHttp2Upstream::send_host(const char* host, hlen_t host_len)
{
	return http2->add_header(ctx, kgl_expand_string(":authority"), host, host_len);
}
bool KHttp2Upstream::send_content_length(int64_t content_length)
{
	char tmpbuff[50];
	int len = snprintf(tmpbuff, sizeof(tmpbuff) - 1, INT64_FORMAT, content_length);
	return http2->add_header(ctx, kgl_expand_string("Content-Length"), tmpbuff, len);
}
bool KHttp2Upstream::send_header_complete(int64_t post_body_len)
{
	return true;
}
#endif
