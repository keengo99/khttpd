#include "KHttpServer.h"
#include "KPreRequest.h"
#include "KHttp2.h"
#include "KSink.h"
#include "KHttp3.h"
#include "klog.h"


KACCEPT_CALLBACK_DECLEAR(handle_connection);

kconnection_start_func server_on_new_connection = NULL;
krequest_start_func server_on_new_request = NULL;
krequest_start_func http2https_error = NULL;
khttp_server_config http_config;

#ifdef KSOCKET_SSL
static bool ssl_is_quic(SSL* ssl) {
#ifdef ENABLE_HTTP3
	const uint8_t* data;
	size_t param = 0;
	SSL_get_peer_quic_transport_params(ssl, &data, &param);
	return param > 0;
#else
	return false;
#endif
}
void khttp_server_alpn(SSL* ssl, void* ssl_ctx_data, const unsigned char** out, unsigned int* outlen)
{
	u_char* alpn = (u_char*)ssl_ctx_data;
#ifdef ENABLE_HTTP3
	if (ssl && ssl_is_quic(ssl)) {
		*out = (unsigned char*)KGL_HTTP_V3_NPN_ADVERTISE;
		*outlen = sizeof(KGL_HTTP_V3_NPN_ADVERTISE) - 1;
		return;
	}
#endif
#ifdef ENABLE_HTTP2
	if (alpn && KBIT_TEST(*alpn, KGL_ALPN_HTTP2)) {
		*out = (unsigned char*)KGL_HTTP_V2_NPN_ADVERTISE KGL_HTTP_NPN_ADVERTISE;
		*outlen = sizeof(KGL_HTTP_V2_NPN_ADVERTISE KGL_HTTP_NPN_ADVERTISE) - 1;
		return;
	}
#endif
	* out = (unsigned char*)KGL_HTTP_NPN_ADVERTISE;
	*outlen = sizeof(KGL_HTTP_NPN_ADVERTISE) - 1;
}
#endif
int khttp_server_new_request(void* arg, int got)
{
	KSink* sink = (KSink*)arg;
	if (!sink->begin_request()) {
		KBIT_SET(sink->data.flags, RQ_CONNECTION_CLOSE);
		sink->end_request();
		return 0;
	}
	server_on_new_request(sink, got);
	return 0;
}
void init_http_server_callback(kconnection_start_func on_new_connection, krequest_start_func on_new_request)
{
	kgl_init_sink_queue();
	init_time_zone();
	memset(&http_config, 0, sizeof(http_config));
	http_config.time_out = 60;
	server_on_new_connection = on_new_connection;
	server_on_new_request = on_new_request;
#ifdef KSOCKET_SSL
	kssl_set_npn_callback(khttp_server_alpn);
#ifdef ENABLE_HTTP3
	init_khttp3();
#endif
#endif
}
bool start_http_server(kserver* server, int flags, SOCKET sockfd)
{
	if (ksocket_opened(sockfd)) {
		return kserver_open_exsit(server, sockfd, handle_connection);
	}
	return kserver_open(server, 0, handle_connection);
}
