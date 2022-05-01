#ifndef KHTTPSERVER_H_99
#define KHTTPSERVER_H_99
#include "kfiber.h"
#include "kconnection.h"
typedef struct _khttp_server_config
{
	int fiber_stack_size;
	int time_out;
} khttp_server_config;

typedef enum {
	kgl_connection_success,
	kgl_connection_too_many,
	kgl_connection_per_limit,
	kgl_connection_unknow
} kgl_connection_result;
class KSink;
typedef kgl_connection_result(*kconnection_start_func)(kconnection* cn);
typedef void (*krequest_start_func)(KSink* sink, int header_len);
extern kconnection_start_func server_on_new_connection;
extern krequest_start_func server_on_new_request;
extern krequest_start_func http2https_error;
extern khttp_server_config http_config;
int khttp_server_new_request(void* arg, int got);
void init_http_server_callback(kconnection_start_func on_new_connection, krequest_start_func on_new_request);
bool start_http_server(kserver* server, int flags);
#endif
