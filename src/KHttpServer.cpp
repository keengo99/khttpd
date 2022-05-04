#include "KHttpServer.h"
#include "KPreRequest.h"
#include "KHttp2.h"
#include "KSink.h"

KACCEPT_CALLBACK_DECLEAR(handle_connection);

kconnection_start_func server_on_new_connection = NULL;
krequest_start_func server_on_new_request = NULL;
krequest_start_func http2https_error = NULL;
khttp_server_config http_config;
static void kgl_ssl_npn(void* ssl_ctx_data, const unsigned char** out, unsigned int* outlen)
{
#ifdef ENABLE_HTTP2
	bool* http2 = (bool*)ssl_ctx_data;
	if (http2 && *http2) {
		*out = (unsigned char*)KGL_HTTP_V2_NPN_ADVERTISE KGL_HTTP_NPN_ADVERTISE;
		*outlen = sizeof(KGL_HTTP_V2_NPN_ADVERTISE KGL_HTTP_NPN_ADVERTISE) - 1;
		return;
	}
#endif
	*out = (unsigned char*)KGL_HTTP_NPN_ADVERTISE;
	*outlen = sizeof(KGL_HTTP_NPN_ADVERTISE) - 1;
}
int khttp_server_new_request(void* arg, int got)
{
	KSink* sink = (KSink*)arg;
	sink->begin_request();
	server_on_new_request(sink, got);
	return 0;
}
void init_http_server_callback(kconnection_start_func on_new_connection, krequest_start_func on_new_request)
{
	memset(&http_config, 0, sizeof(http_config));
	http_config.time_out = 60;
	server_on_new_connection = on_new_connection;
	server_on_new_request = on_new_request;
	kssl_set_npn_callback(kgl_ssl_npn);
}
bool start_http_server(kserver* server, int flags, SOCKET sockfd)
{
	if (ksocket_opened(sockfd)){
		return kserver_open_exsit(server, sockfd, handle_connection);
	}
	return kserver_open(server, 0, handle_connection);
}
