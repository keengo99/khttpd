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
	virtual uint32_t get_server_model() override
	{
		return get_server()->flags;
	}
	virtual KOPAQUE get_server_opaque() override
	{
		return kserver_get_opaque(get_server());
	}
	kselector* get_selector() override
	{
		return get_connection()->st.selector;
	}
	kgl_pool_t* get_connection_pool() override
	{
		return get_connection()->pool;
	}
#ifdef KSOCKET_SSL
	kssl_session* get_ssl() override {
		kconnection* cn = get_connection();
		return cn->st.ssl;
	}
#endif
	sockaddr_i* get_peer_addr() override
	{
		kconnection* cn = get_connection();
		return &cn->addr;
	}
	bool get_self_addr(sockaddr_i* addr) override
	{
		return 0 == kconnection_self_addr(get_connection(), addr);
	}
#ifdef ENABLE_PROXY_PROTOCOL
	kgl_proxy_protocol* get_proxy_info() override
	{
		return get_connection()->proxy;
	}
#endif
	void* get_sni() override
	{
		auto cn = get_connection();
		void* sni = cn->sni;
		cn->sni = NULL;
		return sni;
	}
protected:
	virtual kconnection* get_connection() = 0;
	virtual kserver* get_server()
	{
		return get_connection()->server;
	}
};
#endif
