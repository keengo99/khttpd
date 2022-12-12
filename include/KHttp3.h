#ifndef KGL_HTTP3_H
#define KGL_HTTP3_H
#include "ksocket.h"
#include "khttp.h"
#include "KAtomCountable.h"
#include "kconnection.h"
#include "kserver.h"
#include "kselector_manager.h"
#ifdef ENABLE_HTTP3
#include "lsquic.h"
#define MAX_QUIC_UDP_SIZE 4096
bool init_khttp3();
struct lsquic_conn_ctx
{

};
class KHttp3Server;

class KHttp3ServerEngine
{
public:
	KHttp3ServerEngine(KHttp3Server *server)
	{
		uc = nullptr;
		engine = nullptr;
		selector_tick = NULL;
		udp_buffer = (char *)xmalloc(MAX_QUIC_UDP_SIZE);
		seq = rand();
		this->server = server;
	}
	bool is_server_model()
	{
		return true;
	}
	void ticked()
	{
		if (lsquic_engine_has_unsent_packets(engine)) {
			lsquic_engine_send_unsent_packets(engine);
		}
		lsquic_engine_process_conns(engine);
	}
	bool has_active_connection()
	{
		int diff;
		return lsquic_engine_earliest_adv_tick(engine, &diff) > 0;
	}
	bool is_multi();
	char* realloc_buffer()
	{
		char* old_buffer = udp_buffer;
		udp_buffer = (char*)xmalloc(MAX_QUIC_UDP_SIZE);
		return old_buffer;
	}
	int start();
	int init();
	void release();
	int shutdown();
	char* udp_buffer;
	kconnection* uc;
	lsquic_engine* engine;
	uint32_t seq;
	KHttp3Server* server;
	kselector_tick* selector_tick;
	friend class KHttp3Server;
protected:
	~KHttp3ServerEngine()
	{
		if (uc) {
			kconnection_destroy(uc);
		}
		if (engine) {
			lsquic_engine_destroy(engine);
		}
		xfree(udp_buffer);
		assert(selector_tick == NULL);
	}
};
class KHttp3Server : public KAtomCountable
{
public:
	KHttp3Server(int count)
	{
		flags = 0;
		ssl_ctx = nullptr;
		free_opaque = nullptr;
		data = NULL;
		engine_count = count;
		engines = (KHttp3ServerEngine **)malloc(sizeof(KHttp3ServerEngine*)*count);
		for (int i = 0; i < count; i++) {
			engines[i] = new KHttp3ServerEngine(this);
		}
	}
	void bind_data(kserver_free_opaque free_opaque, KOPAQUE data) {
		try_free_data();
		this->data = data;
		this->free_opaque = free_opaque;
	}
	KOPAQUE get_data()
	{
		return data;
	}
	int init(const char* ip, uint16_t port, kgl_ssl_ctx* ssl_ctx,uint32_t model);
	int init_engine(int index);
	bool start();
	int shutdown();
	bool is_shutdown()
	{
		return KBIT_TEST(flags,KGL_SERVER_START)==0;
	}
	int start_engine(int index);
	int get_engine_index(uint8_t port_id)
	{
		return (int)port_id % engine_count;
	}
	KHttp3ServerEngine* refs_engine(int index)
	{
		assert(index < engine_count);
		addRef();
		return engines[index];
	}
	friend class KHttp3ServerEngine;
	sockaddr_i addr;
	kgl_ssl_ctx* ssl_ctx;
	uint32_t flags;
protected:	
	int engine_count;
	kserver_free_opaque free_opaque;
	KOPAQUE data;
	void try_free_data()
	{
		if (data && free_opaque) {
			free_opaque(data);
		}
	}
	~KHttp3Server()
	{
		if (engines) {
			for (int i = 0; i < engine_count; i++) {
				delete engines[i];
			}
			free(engines);
		}
		if (ssl_ctx) {
			kgl_release_ssl_ctx(ssl_ctx);
		}
		try_free_data();
	}
	KHttp3ServerEngine** engines;
};
KHttp3Server* kgl_h3_new_server(const char* ip, uint16_t port, kgl_ssl_ctx* ssl_ctx, uint32_t model);
#endif
#endif
