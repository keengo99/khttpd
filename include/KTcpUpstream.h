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
		assert(cn);
	}
	~KTcpUpstream()
	{
		if (cn) {
			kfiber_net_close(cn);
		}
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
	bool send_header(const char* attr, hlen_t attr_len, const char* val, hlen_t val_len);
	bool send_method_path(uint16_t meth, const char* path, hlen_t path_len);
	bool send_host(const char* host, hlen_t host_len);
	bool send_content_length(int64_t content_length);
	bool send_header_complete(int64_t post_body_len);
	bool set_header_callback(void* arg, kgl_header_callback cb);
	KGL_RESULT read_header();
	int Read(char* buf, int len);
	int Write(WSABUF* buf, int bc);
	void BindSelector(kselector *selector);
	void gc(int life_time,time_t last_recv_time);
	void OnPushContainer();
	void Destroy()
	{
		delete this;
	}
private:
	kconnection *cn;
	KUpstreamCallBack stack;
};
#endif
