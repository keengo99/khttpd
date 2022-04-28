#include "KTcpSink.h"
#include "KRequest.h"
#include "kfiber.h"
#include "KHttpServer.h"

KTcpSink::KTcpSink(kconnection *cn)
{
	this->cn = cn;
}
KTcpSink::~KTcpSink()
{
	kconnection_destroy(cn);
}
kev_result KTcpSink::StartRequest(KRequest *rq)
{
	assert(rq->raw_url.host == NULL);
	sockaddr_i addr;
	GetSelfAddr(&addr);
	rq->raw_url.port = ksocket_addr_port(&addr);
	int host_len = MAXIPLEN + 9;
	rq->raw_url.host = (char *)malloc(host_len);
	memset(rq->raw_url.host, 0, host_len);
	ksocket_sockaddr_ip(&addr, rq->raw_url.host, MAXIPLEN - 1);
	int len = (int)strlen(rq->raw_url.host);
	snprintf(rq->raw_url.host + len, 7, ".%d", rq->raw_url.port);
	rq->raw_url.path = strdup("/");
	rq->req.meth = METH_CONNECT;
	kfiber_create(server_on_new_request, rq, 0, http_config.fiber_stack_size, NULL);
	return kev_ok;
}
int KTcpSink::EndRequest(KRequest *rq)
{
	delete rq;
	return 0;
}