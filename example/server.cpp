#include <stdio.h>
#include "KHttpServer.h"
#include "KSink.h"
#include "kselector_manager.h"

int on_new_http_request(void* arg, int header_size)
{
	KSink* rq = (KSink*)arg;
	rq->response_status(STATUS_OK);
	rq->response_content_length(4);
	rq->response_connection();
	rq->start_response_body(4);
	rq->write(kgl_expand_string("test"));
	rq->end_request();
	return 0;
}
kev_result on_ready(KOPAQUE data, void* arg, int got)
{
	init_http_server_callback(NULL, on_new_http_request);
	http_config.time_out = 30;
	kserver* server = kserver_init();
	kserver_bind(server, "127.0.0.1", 8888, NULL);
	start_http_server(server);
	kserver_release(server);
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