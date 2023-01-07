#include "KPreRequest.h"
#include "KHttpSink.h"
#include "KTcpSink.h"
#include "KRequest.h"
#include "KHttp2.h"
#include "KProxy.h"
#include "kfiber.h"
#include "KHttpServer.h"


kev_result handle_ssl_accept(KOPAQUE data, void *arg, int got);
#define KGL_BUSY_MSG "HTTP/1.0 503 Service Unavailable\r\nConnection: close\r\n\r\nServer is busy."
static kev_result handle_http_request(kconnection *cn)
{
#ifdef WORK_MODEL_TCP
	if (KBIT_TEST(cn->server->flags, WORK_MODEL_TCP)) {
		KTcpSink *sink = new KTcpSink(cn, NULL);
		selectable_bind_opaque(&cn->st, (KSink *)sink);
		return sink->read_header();
	}
#endif
	KHttpSink *sink = new KHttpSink(cn,NULL);
	selectable_bind_opaque(&cn->st, (KSink*)sink);
	return sink->read_header();
}

static kev_result handle_ssl_proxy_callback(KOPAQUE data, void *arg, int got)
{
	kconnection *c = (kconnection *)arg;
	if (got == 0) {
		return handle_http_request((kconnection *)arg);
	}
	kconnection_destroy(c);
	return kev_destroy;
}
static kev_result handle_request(kconnection *c)
{
#ifdef KSOCKET_SSL
	if (kconnection_is_ssl(c)) {
		return kconnection_ssl_handshake(c, handle_ssl_accept, c);
	}
#endif
	return handle_http_request(c);
}
static kev_result handle_first_package_ready(KOPAQUE data, void *arg, int got)
{
	kconnection *c = (kconnection *)arg;
	u_char buf[2];
	int n = recv(c->st.fd, (char *)buf, 1, MSG_PEEK);
	if (n <= 0) {
		kconnection_destroy(c);
		return kev_destroy;
	}
	if (buf[0] & 0x80 /* SSLv2 */ || buf[0] == 0x16 /* SSLv3/TLSv1 */) {
		return handle_request(c);
	}
	return handle_http_request(c);
}
static kev_result handle_http_https_request(kconnection *c)
{
#ifdef KSOCKET_SSL
	if (kconnection_is_ssl(c)) {
		return selectable_read(&c->st, handle_first_package_ready, NULL, c);
	}
#endif
	return handle_http_request(c);
}
static kev_result result_proxy_request(KOPAQUE data, void *arg, int got)
{
	kconnection *cn = (kconnection *)arg;
	if (got < 0) {
		kconnection_destroy(cn);
		return kev_destroy;
	}
	return handle_http_https_request(cn);
}

kev_result handle_ssl_accept(KOPAQUE data, void *arg,int got)
{
	kconnection *cn = (kconnection *)arg;
	if (got < 0) {
		kconnection_destroy(cn);
		return kev_destroy;
	}
#if defined(TLSEXT_TYPE_next_proto_neg) && defined(ENABLE_HTTP2)
	kassert(cn && cn->st.ssl);
	const unsigned char *protocol_data = NULL;
	unsigned len = 0;
	kgl_ssl_get_next_proto_negotiated(cn->st.ssl->ssl, &protocol_data, &len);
	if (len == sizeof(KGL_HTTP_V2_NPN_NEGOTIATED) - 1 &&
		memcmp(protocol_data, KGL_HTTP_V2_NPN_NEGOTIATED, len) == 0) {
		KHttp2 *http2 = new KHttp2();
		selectable_bind_opaque(&cn->st, http2);
		http2->server(cn);
		return kev_ok;
	}
#endif
#ifdef HTTP_PROXY
	if (KBIT_TEST(cn->server->flags, WORK_MODEL_SSL_PROXY)) {
		return handl_proxy_request(cn, handle_ssl_proxy_callback);
	}
#endif
	return handle_http_request(cn);
}
KACCEPT_CALLBACK (handle_connection){
	kconnection* c = (kconnection*)arg;
	if (server_on_new_connection) {
		kgl_connection_result result = server_on_new_connection(c);
		if (unlikely(result != kgl_connection_success)) {
			kconnection_destroy(c);
			return kev_ok;
		}
	}
#ifdef ENABLE_PROXY_PROTOCOL
	if (KBIT_TEST(c->server->flags, WORK_MODEL_PROXY)) {
		handl_proxy_request(c, result_proxy_request);
		return kev_ok;
	}
#ifdef HTTP_PROXY
	if (kconnection_is_ssl(c) && KBIT_TEST(c->server->flags, WORK_MODEL_SSL_PROXY)) {
		handle_request(c);
		return kev_ok;
	}
#endif
#endif
	handle_http_https_request(c);
	return kev_ok;
}
