#ifndef KTSUPSTREAM_H
#define KTSUPSTREAM_H
#include "KUpstream.h"



//thread safe upstream
class KTsUpstream : public KUpstream {
public:
	KTsUpstream(KUpstream *us)
	{
		this->us = us;
		header = NULL;
	}
	~KTsUpstream()
	{
		kassert(us == NULL);
	}
	void SetDelay()
	{
		us->SetDelay();
	}
	void SetNoDelay(bool forever)
	{
		us->SetNoDelay(forever);
	}
	kconnection *GetConnection()
	{
		return us->GetConnection();
	}
	void SetTimeOut(int tmo)
	{
		return us->SetTimeOut(tmo);
	}
	void set_content_length(int64_t content_length)
	{
		return us->set_content_length(content_length);
	}
	bool send_connection(const char* val, hlen_t val_len)
	{
		return us->send_connection(val, val_len);
	}
	bool send_host(const char* host, hlen_t host_len)
	{
		return us->send_host(host, host_len);
	}
	bool send_method_path(uint16_t meth, const char* path, hlen_t path_len)
	{
		return us->send_method_path(meth, path, path_len);
	}
	KGL_RESULT send_header_complete();
	bool set_header_callback(void* arg, kgl_header_callback header_callback);
	KGL_RESULT read_header();
	KOPAQUE GetOpaque()
	{
		return us->GetOpaque();
	}
	void BindOpaque(KOPAQUE data)
	{
		us->BindOpaque(data);
	}
	int read(WSABUF* buf, int bc);
	int write(WSABUF* buf, int bc);
	bool send_header(const char* attr, hlen_t attr_len, const char* val, hlen_t val_len)
	{
		return us->send_header(attr, attr_len, val, val_len);
	}
	bool send_method_path(uint16_t meth, const char* path, int path_len)
	{
		return us->send_method_path(meth, path, path_len);
	}
	bool IsMultiStream()
	{
		return us->IsMultiStream();
	}
	bool IsNew() {
		return us->IsNew();
	}
	int GetLifeTime()
	{
		return us->GetLifeTime();
	}
	void IsGood()
	{
		return us->IsGood();
	}
	void IsBad(BadStage stage)
	{
		return us->IsBad(stage);
	}
	void write_end();
	void Shutdown();
	void Destroy();
	sockaddr_i *GetAddr()
	{
		return us->GetAddr();
	}
	int GetPoolSid()
	{
		return 0;
	}
	kgl_pool_t *GetPool()
	{
		return us->GetPool();
	}
	kgl_refs_string* get_param()
	{
		return us->get_param();
	}
	void gc(int life_time,time_t last_recv_time);
	KUpstreamCallBack stack;
	KUpstream *us;
	KHttpHeader *header;
	KHttpHeader *last_header;
};
#endif
