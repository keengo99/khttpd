#ifndef KTSUPSTREAM_H
#define KTSUPSTREAM_H
#include "KUpstream.h"



//thread safe upstream
class KTsUpstream : public KUpstream
{
public:
	KTsUpstream(KUpstream* us) {
		this->us = us;
		header = NULL;
	}
	~KTsUpstream() {
		kassert(us == NULL);
	}
	void set_delay() override {
		us->set_delay();
	}
	void set_no_delay(bool forever) override {
		us->set_no_delay(forever);
	}
	kconnection* get_connection() override {
		return us->get_connection();
	}
	void set_time_out(int tmo) override {
		return us->set_time_out(tmo);
	}
	void set_content_length(int64_t content_length) override {
		return us->set_content_length(content_length);
	}
	bool send_connection(const char* val, hlen_t val_len) override {
		return us->send_connection(val, val_len);
	}
	bool send_host(const char* host, hlen_t host_len) override {
		return us->send_host(host, host_len);
	}
	bool send_method_path(uint16_t meth, const char* path, hlen_t path_len) override {
		return us->send_method_path(meth, path, path_len);
	}
	KGL_RESULT send_header_complete() override;
	bool set_header_callback(void* arg, kgl_header_callback header_callback) override;
	KGL_RESULT read_header() override;
#if 0
	int64_t get_left() override {
		return us->get_left();
	}
#endif
	KOPAQUE GetOpaque() override {
		return us->GetOpaque();
	}
	void BindOpaque(KOPAQUE data) override {
		us->BindOpaque(data);
	}
	KHttpHeader* get_trailer() override {
		return us->get_trailer();
	}
	int read(char* buf, int len) override;
	int write(kgl_iovec* buf, int bc) override;
	bool send_trailer(const char* name, hlen_t name_len, const char* val, hlen_t val_len) override {
		return us->send_trailer(name, name_len, val, val_len);
	}
	bool send_header(kgl_header_type attr, const char* val, hlen_t val_len) override {
		return us->send_header(attr, val, val_len);
	}
	bool send_header(const char* attr, hlen_t attr_len, const char* val, hlen_t val_len) override {
		return us->send_header(attr, attr_len, val, val_len);
	}
	bool IsMultiStream() override {
		return us->IsMultiStream();
	}
	bool IsNew() override {
		return us->IsNew();
	}
	int GetLifeTime() override {
		return us->GetLifeTime();
	}
	void health(HealthStatus stage) override {
		return us->health(stage);
	}
	void write_end() override;
	void shutdown() override;
	void Destroy() override;
	sockaddr_i* GetAddr() override {
		return us->GetAddr();
	}
	int GetPoolSid() {
		return 0;
	}
	kgl_pool_t* GetPool() override {
		return us->GetPool();
	}
	kgl_refs_string* get_param() override {
		return us->get_param();
	}
	KPoolableSocketContainer* get_container() override {
		return us->get_container();
	}
	void gc(int life_time) override;
	KUpstreamCallBack stack;
	KUpstream* us;
	KHttpHeader* header;
	KHttpHeader* last_header;
};
#endif
