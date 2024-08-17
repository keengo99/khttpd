#include <stdio.h>
#include "KHttpServer.h"
#include "KSink.h"
#include "kselector_manager.h"
#include "KSockPoolHelper.h"
#include "KHttp3.h"

void on_new_http_request(KSink *rq, int header_size)
{
	rq->response_status(STATUS_OK);
	rq->response_content_length(4);
	rq->response_connection();
	rq->response_header(kgl_expand_string("x-server"), kgl_expand_string("test"));
	rq->start_response_body(4);
	rq->write(kgl_expand_string("test"));
}
static bool us_header(KUpstream *us, void* arg, const char* attr, int attr_len, const char* val, int val_len,bool is_first)
{
	if (is_first) {
		printf("%s %s\r\n", attr, val);
		return true;
	}
	printf("%s: %s\n", attr,val);
	return true;
}
int client_http_test(void *arg,int got)
{
	KSockPoolHelper server;
	server.setHostPort("dss0.bdstatic.com", "443sp");
	KUpstream* us = server.get_upstream(0);
	if (us == NULL) {
		printf("connect failed.\n");
		return -1;
	}
	us->BindOpaque(&server);
	us->set_header_callback(NULL, us_header);
	us->send_method_path(METH_GET, kgl_expand_string("/5aV1bjqh_Q23odCf/static/newmusic/css/newmusic_min_1b1ebf56.css"));
	us->send_host(kgl_expand_string("dss0.bdstatic.com"));
	us->send_header(kgl_expand_string(":scheme"), kgl_expand_string("https"));
	us->set_content_length(0);
	if (KGL_OK != us->send_header_complete()) {
		printf("failed\n");
		us->gc(-1);
		return -1;
	}
	KGL_RESULT result = us->read_header();
	printf("read_header result=[%d]\n", result);
	char buf[512];
	for (;;) {
		int got = us->read(buf, sizeof(buf));
		if (got <= 0) {
			printf("got=[%d]\n", got);
			break;
		}
		printf("got=[%d]\n", got);
	}
	us->gc(-1);
	return 0;
}
#ifdef KSOCKET_SSL
static void* ssl_create_sni(KOPAQUE server_ctx, const char* hostname, SSL_CTX **ssl_ctx)
{
	return NULL;
}
static void ssl_free_sni(void* sni)
{

}
static u_char h3_alpn = KGL_ALPN_HTTP3|KGL_ALPN_HTTP2;
#endif
kgl_ssl_ctx* ssl_ctx = NULL;
#ifdef ENABLE_HTTP3
int h3_server(void* arg, int got)
{
	
	auto h3_server = kgl_h3_new_server("0.0.0.0", 4433, 0, ssl_ctx, 0);
	if (h3_server == nullptr) {
		perror("cann't init h3 server");
		return -1;
	}
	h3_server->start();
	kfiber_msleep(5000);
	//printf("now shutdown h3 test engine\n");
	//h3_server->shutdown();
	return 0;
}
#endif
kev_result on_ready(KOPAQUE data, void* arg, int got)
{	
	http_config.time_out = 30;
#ifdef KSOCKET_SSL
	ssl_ctx = kgl_new_ssl_ctx(kgl_ssl_ctx_new_server("server.crt", "server.key", NULL, NULL, &h3_alpn));
#endif
	kserver* server = kserver_init();
	kserver_bind(server, "0.0.0.0", 4433, ssl_ctx);
	KBIT_SET(server->flags, WORK_MODEL_ALT_H3);
	start_http_server(server,0);
	kserver_release(server);
#ifdef ENABLE_HTTP3
	kfiber_create(h3_server, NULL, 0, 0, NULL);
#endif
	return kev_ok;
}
int main(int argc, char** argv)
{
	kasync_init();
#ifdef KSOCKET_SSL
	kssl_set_sni_callback(ssl_create_sni, ssl_free_sni);
#endif
	init_http_server_callback(NULL, on_new_http_request);
	selector_manager_on_ready(on_ready, NULL);
	selector_manager_init(1, true);
	selector_manager_set_timeout(10, 10);
	selector_manager_start(NULL, false);
}
