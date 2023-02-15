#ifndef KHTTPSERVER_H_99
#define KHTTPSERVER_H_99
#include "kfiber.h"
#include "kconnection.h"
#include "kstring.h"

typedef struct _khttp_server_config
{
	int fiber_stack_size;
	int time_out;
} khttp_server_config;

typedef int kgl_connection_result;
constexpr kgl_connection_result kgl_connection_unknow = 0;
constexpr kgl_connection_result kgl_connection_too_many = 1;
constexpr kgl_connection_result kgl_connection_per_limit = 2;
constexpr kgl_connection_result kgl_connection_no_keep_alive = 4;
constexpr kgl_connection_result kgl_connection_success = 8;

class KSink;
typedef kgl_connection_result(*kconnection_start_func)(kconnection* cn);
typedef void (*krequest_start_func)(KSink* sink, int header_len);
extern kconnection_start_func server_on_new_connection;
extern krequest_start_func server_on_new_request;
extern krequest_start_func http2https_error;
extern khttp_server_config http_config;
void khttp_server_set_ssl_config(const char* ca_path, const char* ssl_client_chiper, const char* ssl_client_protocols);
void khttp_server_refs_ssl_config(kgl_refs_string** ca_path, kgl_refs_string** ssl_client_chiper, kgl_refs_string** ssl_client_protocols);
void khttp_server_alpn(void* ssl_ctx_data, const unsigned char** out, unsigned int* outlen);
int khttp_server_new_request(void* arg, int got);
void init_http_server_callback(kconnection_start_func on_new_connection, krequest_start_func on_new_request);
void shutdown_http_server();
bool start_http_server(kserver* server, int flags, SOCKET sockfd = INVALID_SOCKET);
#endif
