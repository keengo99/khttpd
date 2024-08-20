#ifndef KHTTPSINK_H_99
#define KHTTPSINK_H_99
#include "KTcpServerSink.h"
#include "kconnection.h"
#include "KHttpParser.h"
#include "KResponseContext.h"
#include "KHttpHeader.h"
#include "KDechunkContext.h"
#include "KHttpServer.h"
//handle http/1.x protocol
class KHttpSink : public KSingleConnectionSink
{
public:
	KHttpSink(kconnection* c, kgl_pool_t* pool);
	~KHttpSink();
	bool is_locked() override
	{
		return KBIT_TEST(cn->st.base.st_flags, STF_LOCK);
	}
	bool response_header(kgl_header_type know_header, const char* val, int val_len, bool lock_value) override;
	bool response_header(const char* name, int name_len, const char* val, int val_len) override;
	bool response_connection(const char* val, int val_len) override {
		return response_header(kgl_header_connection, val, val_len, true);
	}
	bool readhup(void* arg, result_callback result) override
	{
		return selectable_readhup(&cn->st, result, arg);
	}
	void remove_readhup() override
	{
		selectable_remove_readhup(&cn->st);
	}
	void set_delay() override
	{
		ksocket_delay(cn->st.fd);
	}
	void set_no_delay(bool forever) override
	{
		ksocket_no_delay(cn->st.fd, forever);
	}
	void shutdown() override
	{
		selectable_shutdown(&cn->st);
	}
	void set_time_out(int tmo) override
	{
		cn->st.base.tmo = tmo;
		cn->st.base.tmo_left = tmo;
	}
	int get_time_out() override
	{
		return cn->st.base.tmo;
	}
	/* return true will use pipe_line */
	bool end_request();
	ks_buffer buffer;
	bool read_header();
	kgl_parse_result parse();
	KResponseContext* rc;
	kconnection* get_connection() override
	{
		return cn;
	}
	KDechunkContext* GetDechunkContext()
	{
		return dechunk;
	}
	KHttpHeader* get_trailer() override {
		if (!dechunk || !dechunk->trailer) {
			return nullptr;
		}
		return dechunk->trailer->header;
	}
	bool response_trailer(const char* name, int name_len, const char* val, int val_len) override;
	void start(int header_len) override;
	bool skip_post();
	bool start_pipe_line();
	int sendfile(kfiber_file* fp, int len) override;
	int write_all(const char* buf, int len) override;
	int write_all(const kbuf* buf, int len) override;
protected:
	bool response_altsvc_header(const char* val, int val_len) override
	{
		return response_header(kgl_header_alt_svc, val, val_len, false);
	}
	int internal_start_response_body(int64_t body_size, bool is_100_continue) override;
	int internal_read(char* buf, int len) override;
	int internal_write(const kbuf* buf, int len,const kgl_iovec *suffix) {
		if (rc && !rc->ab.empty()) {
			rc->ab.attach_buffers(buf, len);
			int left = KSingleConnectionSink::write_buf(rc->ab.getHead(),rc->ab.getLen(), suffix);
			delete rc;
			rc = nullptr;
			on_success_response(len - left);
			return left;
		}
		int left = KSingleConnectionSink::write_buf(buf, len, suffix);
		on_success_response(len - left);
		return left;
	}
protected:
#ifdef ENABLE_HTTP2
	bool switch_h2c();
#endif
	bool internal_response_status(uint16_t status_code) override;
	KDechunkContext* dechunk;
	khttp_parser parser;
};
#endif

