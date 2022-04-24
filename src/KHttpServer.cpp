#include "KHttpServer.h"
#include "KPreRequest.h"

kconnection_start_func server_on_new_connection = NULL;
kfiber_start_func server_on_new_request = NULL;
khttp_server_config http_config;
void init_http_server_callback(kconnection_start_func on_new_connection, kfiber_start_func on_new_request)
{
	memset(&http_config, 0, sizeof(http_config));
	http_config.time_out = 60;
	server_on_new_connection = on_new_connection;
	server_on_new_request = on_new_request;
}
bool start_http_server(kserver* server)
{
	return kserver_open(server, 0, handle_connection);
}
