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

static kgl_ref_str_t* ca_path = nullptr;
static kgl_ref_str_t* ssl_client_chiper = nullptr;
static kgl_ref_str_t* ssl_client_protocols = nullptr;
static kmutex ssl_config_lock;


void khttp_server_set_ssl_config(const char* ca_path, const char* ssl_client_chiper, const char* ssl_client_protocols) {
	kmutex_lock(&ssl_config_lock);
	kstring_release(::ca_path);
	::ca_path = kstring_from(ca_path);
	kstring_release(::ssl_client_chiper);
	::ssl_client_chiper = kstring_from(ssl_client_chiper);
	kstring_release(::ssl_client_protocols);
	::ssl_client_protocols = kstring_from(ssl_client_protocols);
	kmutex_unlock(&ssl_config_lock);
}
void khttp_server_refs_ssl_config(kgl_ref_str_t** ca_path, kgl_ref_str_t** ssl_client_chiper, kgl_ref_str_t** ssl_client_protocols) {
	kmutex_lock(&ssl_config_lock);
	*ca_path = kstring_refs(::ca_path);
	*ssl_client_chiper = kstring_refs(::ssl_client_chiper);
	*ssl_client_protocols = kstring_refs(::ssl_client_protocols);
	kmutex_unlock(&ssl_config_lock);
}

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
void khttp_server_alpn(SSL* ssl, void* ssl_ctx_data, const unsigned char** out, unsigned int* outlen) {
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
int khttp_server_new_request(KSink *sink, int got) {
	if (!sink->begin_request()) {
		KBIT_SET(sink->data.flags, RQ_CONNECTION_CLOSE);
		return 0;
	}
	server_on_new_request(sink, got);
	return 0;
}
void init_http_server_callback(kconnection_start_func on_new_connection, krequest_start_func on_new_request) {
	kmutex_init(&ssl_config_lock, NULL);
	kgl_init_sink_queue();
	init_time_zone();
	kgl_init_header_string();
	memset(&http_config, 0, sizeof(http_config));
	http_config.time_out = 60;
	server_on_new_connection = on_new_connection;
	server_on_new_request = on_new_request;
#ifdef KSOCKET_SSL
	kssl_set_npn_callback(khttp_server_alpn);
#ifdef ENABLE_HTTP3
	kgl_init_khttp3();
#endif
#endif
}
bool start_http_server(kserver* server, int flags, SOCKET sockfd) {
	if (ksocket_opened(sockfd)) {
		return kserver_open_exsit(server, sockfd, handle_connection);
	}
	return kserver_open(server, flags, handle_connection);
}
void shutdown_http_server() {
#ifdef ENABLE_HTTP3
	kgl_shutdown_khttp3();
#endif
	khttp_server_set_ssl_config(NULL, NULL, NULL);
}