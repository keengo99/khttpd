#ifndef KTCPUPSTREAM_H
#define KTCPUPSTREAM_H
#include "KUpstream.h"
#include "kconnection.h"
#include "kfiber.h"

class KTcpUpstream : public KUpstream {
public:
	KTcpUpstream(kconnection *cn)
	{
		this->cn = cn;
		pool = NULL;
		assert(cn);
	}
	~KTcpUpstream()
	{
		if (cn) {
			kfiber_net_close(cn);
		}
		assert(pool == NULL);
	}
	void BindOpaque(KOPAQUE data)
	{
		selectable_bind_opaque(&cn->st, data, kgl_opaque_other);
	}
	sockaddr_i *GetAddr()
	{
		if (cn) {
			return &cn->addr;
		}
		return NULL;
	}
	void EmptyConnection()
	{
		this->cn = NULL;
	}
	kconnection *GetConnection()
	{
		return this->cn;
	}
	void SetDelay()
	{
		ksocket_delay(cn->st.fd);
	}
	void SetNoDelay(bool forever)
	{
		ksocket_no_delay(cn->st.fd,forever);
	}
	void Shutdown()
	{
		selectable_shutdown(&cn->st);
	}
	void SetTimeOut(int tmo)
	{
		cn->st.tmo = tmo;
		cn->st.tmo_left = tmo;
	}
	bool IsLocked()
	{
		return KBIT_TEST(cn->st.st_flags, STF_LOCK)>0;
	}
	bool send_connection(const char* val, hlen_t val_len)
	{
		return true;
	}
	bool send_header(const char* attr, hlen_t attr_len, const char* val, hlen_t val_len) override;
	bool send_method_path(uint16_t meth, const char* path, hlen_t path_len) override;
	bool send_host(const char* host, hlen_t host_len) override;
	void set_content_length(int64_t content_length) override
	{

	}
	KGL_RESULT send_header_complete() override;
	bool set_header_callback(void* arg, kgl_header_callback cb) override;
	KGL_RESULT read_header() override;
	int read(char* buf, int len) override;
	int write(WSABUF* buf, int bc) override;
	void bind_selector(kselector *selector) override;
	void gc(int life_time) override;
	void unbind_selector() override;
	void Destroy() override
	{
		delete this;
	}
	void clean()
	{
		if (pool) {
			kgl_destroy_pool(pool);
			pool = NULL;
		}
		memset(&stack, 0, sizeof(stack));
	}
	kgl_pool_t* GetPool()
	{
		if (pool == NULL) {
			pool = kgl_create_pool(8192);
		}
		return pool;
	}
protected:
	kconnection *cn;
	KUpstreamCallBack stack;
	kgl_pool_t* pool;
};
#endif
