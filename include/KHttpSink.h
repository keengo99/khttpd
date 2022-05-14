#ifndef KHTTPSINK_H_99
#define KHTTPSINK_H_99
#include "KSink.h"
#include "kconnection.h"
#include "KHttpParser.h"
#include "KResponseContext.h"
#include "KHttpHeader.h"
#include "KDechunkContext.h"
//处理http1协议的
int buffer_read_http_sink(KOPAQUE data, void *arg, LPWSABUF buf, int bufCount);

class KHttpSink : public KSink {
public:
	KHttpSink(kconnection *c,kgl_pool_t *pool);
	~KHttpSink();
	bool is_locked() override
	{
		return KBIT_TEST(cn->st.st_flags, STF_LOCK);
	}
	bool response_header(const char *name, int name_len, const char *val, int val_len) override;
	bool response_connection(const char *val, int val_len) override {
		return response_header(kgl_expand_string("Connection"), val, val_len);
	}

	bool read_hup(void *arg, result_callback result) override
	{
		return selectable_readhup(&cn->st, result, arg);
	}
	void remove_read_hup() override
	{
		selectable_remove_readhup(&cn->st);
	}
	void add_sync() override
	{
		selectable_add_sync(&cn->st);
	}
	void remove_sync() override
	{
		selectable_remove_sync(&cn->st);
	}
	void set_delay() override
	{
		ksocket_delay(cn->st.fd);
	}
	void set_no_delay(bool forever) override
	{
		ksocket_no_delay(cn->st.fd,forever);
	}
	void shutdown() override
	{
		selectable_shutdown(&cn->st);
	}
	void set_time_out(int tmo) override
	{
		cn->st.tmo = tmo;
		cn->st.tmo_left = tmo;
	}
	int get_time_out() override
	{
		return cn->st.tmo;
	}
	int end_request() override;
	ks_buffer buffer;
	kev_result ReadHeader();
	kev_result Parse();
	KResponseContext *rc;
	kconnection *cn;
	kev_result ResultResponseContext(int got);
	kconnection *get_connection() override
	{
		return cn;
	}
	KDechunkContext *GetDechunkContext()
	{
		return dechunk;
	}
	void SkipPost();
	int StartPipeLine();
	void EndFiber();
protected:
	int internal_start_response_body(int64_t body_size) override;
	int internal_read(char* buf, int len) override;
	int internal_write(WSABUF* buf, int bc) override;
protected:
	void start_header();
	bool internal_response_status(uint16_t status_code) override;
	KDechunkContext *dechunk;
	khttp_parser parser;
};
#endif

