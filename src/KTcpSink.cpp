#include "KTcpSink.h"
#include "KRequest.h"
#include "kfiber.h"
#include "KHttpServer.h"

KTcpSink::KTcpSink(kconnection *cn,kgl_pool_t *pool) : KSink(pool)
{
	this->cn = cn;
}
KTcpSink::~KTcpSink()
{
	kconnection_destroy(cn);
}
kev_result KTcpSink::StartRequest()
{
	assert(data.raw_url.host == NULL);
	sockaddr_i addr;
	GetSelfAddr(&addr);
	data.raw_url.port = ksocket_addr_port(&addr);
	int host_len = MAXIPLEN + 9;
	data.raw_url.host = (char *)malloc(host_len);
	memset(data.raw_url.host, 0, host_len);
	ksocket_sockaddr_ip(&addr, data.raw_url.host, MAXIPLEN - 1);
	int len = (int)strlen(data.raw_url.host);
	snprintf(data.raw_url.host + len, 7, ".%d", data.raw_url.port);
	data.raw_url.path = strdup("/");
	data.meth = METH_CONNECT;
	kfiber_create(server_on_new_request, (KSink *)this, 0, http_config.fiber_stack_size, NULL);
	return kev_ok;
}
int KTcpSink::end_request()
{
	delete this;
	return 0;
}