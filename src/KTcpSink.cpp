#include "KTcpSink.h"
#include "KRequest.h"
#include "kfiber.h"
#include "KHttpServer.h"

KTcpSink::KTcpSink(kconnection *cn,kgl_pool_t *pool) : KSingleConnectionSink(cn, pool)
{
	this->cn = cn;
}
KTcpSink::~KTcpSink()
{
	kconnection_destroy(cn);
}
void KTcpSink::start(int header_len)
{
	assert(data.raw_url == NULL);
	sockaddr_i addr;
	get_self_addr(&addr);
	data.raw_url = new KUrl;
	data.raw_url->port = ksocket_addr_port(&addr);
	int host_len = MAXIPLEN + 9;
	data.raw_url->host = (char *)malloc(host_len);
	memset(data.raw_url->host, 0, host_len);
	ksocket_sockaddr_ip(&addr, data.raw_url->host, MAXIPLEN - 1);
	int len = (int)strlen(data.raw_url->host);
	snprintf(data.raw_url->host + len, 7, ".%d", data.raw_url->port);
	data.raw_url->path = strdup("/");
	data.meth = METH_CONNECT;
	khttp_server_new_request(this, header_len);
	//kfiber_create(khttp_server_new_request, (KSink *)this, 0, http_config.fiber_stack_size, NULL);
	//return kev_ok;
}
