#ifndef KTCPSINK_H_99
#define KTCPSINK_H_99
#include "KTcpServerSink.h"
#include "kconnection.h"
#include "kfiber.h"

class KTcpSink : public KTcpServerSink
{
public:
	KTcpSink(kconnection *cn,kgl_pool_t *pool);
	~KTcpSink();
	bool is_locked() override
	{
		return KBIT_TEST(cn->st.st_flags, STF_LOCK);
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
	bool read_hup(void *arg, result_callback result) override
	{
		return selectable_readhup(&cn->st, result, arg);
	}
	void remove_read_hup() override
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
	int internal_write(LPWSABUF buf, int bc) override
	{
		return kfiber_net_writev(cn, buf, bc);
	}
	void add_sync() override
	{
		selectable_add_sync(&cn->st);
	}
	void remove_sync() override
	{
		selectable_remove_sync(&cn->st);
	}
	kconnection *get_connection() override
	{
		return cn;
	}
	kev_result StartRequest();
	int end_request() override;
	bool internal_response_status(uint16_t status_code) override
	{
		return false;
	}
	bool response_header(const char *name, int name_len, const char *val, int val_len) override
	{
		return false;
	}
	bool response_connection(const char *val, int val_len) override
	{
		return false;
	}
	int internal_start_response_body(int64_t body_size) override
	{
		return 0;
	}
	void set_delay() override
	{
		ksocket_delay(cn->st.fd);
	}
	void set_no_delay(bool forever) override
	{
		ksocket_no_delay(cn->st.fd, forever);
	}
	kconnection *cn;
};
#endif

