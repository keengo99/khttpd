#ifndef KTCPSINK_H_99
#define KTCPSINK_H_99
#include "KTcpServerSink.h"
#include "kconnection.h"
#include "kfiber.h"

class KTcpSink : public KSingleConnectionSink
{
public:
	KTcpSink(kconnection *cn,kgl_pool_t *pool);
	~KTcpSink();
	bool is_locked() override
	{
		return KBIT_TEST(cn->st.base.st_flags, STF_LOCK);
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
	bool readhup(void *arg, result_callback result) override
	{
		return selectable_readhup(&cn->st, result, arg);
	}
	void remove_readhup() override
	{
		selectable_remove_readhup(&cn->st);
	}
	void shutdown() override
	{
		selectable_shutdown(&cn->st);
	}
	int internal_read(char *buf, int len) override
	{
		return kfiber_net_read(cn, buf, len);
	}
	kconnection *get_connection() override
	{
		return cn;
	}
	void start(int header_len) override;
	bool internal_response_status(uint16_t status_code) override
	{
		return false;
	}
	bool response_header(const char *name, int name_len, const char *val, int val_len) override
	{
		return false;
	}
	int internal_start_response_body(int64_t body_size, bool is_100_continue) override
	{
		return 0;
	}
	void set_delay() override {
		ksocket_delay(cn->st.fd);
	}
	void set_no_delay(bool forever) override {
		ksocket_no_delay(cn->st.fd, forever);
	}
};
#endif

