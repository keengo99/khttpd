#ifndef KHTTP3_CONNECTION_H_INCLUDED
#define KHTTP3_CONNECTION_H_INCLUDED
#include "KAtomCountable.h"
#include "kmalloc.h"
#include "KHttp3.h"
#ifdef ENABLE_HTTP3
class KHttp3Connection : public KAtomCountable
{
public:
	KHttp3Connection(lsquic_conn_t*c, KHttp3ServerEngine *engine)
	{
		engine->add_refs();
		this->engine = engine;
		pool = nullptr;
		this->c = c;
		lsquic_conn_get_sockaddr(c, &local_addr, &peer_addr);
		sni = engine->get_cache_sni(lsquic_conn_get_sni(c));
	}
	kgl_pool_t* get_pool()
	{
		if (pool == nullptr) {
			pool = kgl_create_pool(4096);
		}
		return pool;
	}
	void detach_connection()
	{
		c = nullptr;
	}
	void* get_sni()
	{
		void* data = sni;
		sni = nullptr;
		return data;
	}
	friend class KHttp3Sink;
protected:
	~KHttp3Connection()
	{
		assert(c == nullptr);
		engine->release();
		if (pool) {
			kgl_destroy_pool(pool);
		}
		if (sni && kgl_ssl_free_sni) {
			kgl_ssl_free_sni(sni);
		}		
	}
	const struct sockaddr* local_addr = NULL;
	const struct sockaddr* peer_addr = NULL;
	kgl_pool_t* pool;
	KHttp3ServerEngine* engine;
	lsquic_conn_t* c;
	void* sni;
};
#endif
#endif
