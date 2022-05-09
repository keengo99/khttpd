#include <stdio.h>
#include "KHttpServer.h"
#include "KSink.h"
#include "kselector_manager.h"
#include "KSockPoolHelper.h"

void on_new_http_request(KSink *rq, int header_size)
{
	rq->response_status(STATUS_OK);
	rq->response_content_length(4);
	rq->response_connection();
	rq->response_header(kgl_expand_string("x-server"), kgl_expand_string("test"));
	rq->start_response_body(4);
	rq->write(kgl_expand_string("test"));
	rq->end_request();
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
kev_result on_ready(KOPAQUE data, void* arg, int got)
{
	init_http_server_callback(NULL, on_new_http_request);
	http_config.time_out = 30;
	kserver* server = kserver_init();
	kserver_bind(server, "127.0.0.1", 8888, NULL);
	start_http_server(server,0);
	kserver_release(server);
	//kfiber_create(client_http_test, NULL, 0, 0, NULL);
	return kev_ok;
}
int main(int argc, char** argv)
{
	printf("sizeof(KRequest)=[%d]\n", sizeof(KRequestData));
	kasync_init();
	selector_manager_on_ready(on_ready, NULL);
	selector_manager_init(1, true);
	selector_manager_set_timeout(10, 10);
	selector_manager_start(NULL, false);
}