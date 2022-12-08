#ifndef KTCPSERVERSINK_H
#define KTCPSERVERSINK_H
#include "KSink.h"
class KTcpServerSink : public KSink
{
public:
	KTcpServerSink(kgl_pool_t* pool) : KSink(pool)
	{

	}
	virtual ~KTcpServerSink()
	{

	}
	uint8_t get_server_model() override
	{
		return get_server()->flags;
	}
	virtual KOPAQUE get_server_opaque()
	{
		return kserver_get_opaque(get_server());
	}
protected:
	virtual kconnection* get_connection() = 0;
	virtual kserver* get_server()
	{
		return get_connection()->server;
	}
};
#endif
