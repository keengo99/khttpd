#ifndef KTCPSINK_H_99
#define KTCPSINK_H_99
#include "KSink.h"
#include "kconnection.h"
#include "kfiber.h"

class KTcpSink : public KSink {
public:
	KTcpSink(kconnection *cn,kgl_pool_t *pool);
	~KTcpSink();
	bool IsLocked()
	{
		return KBIT_TEST(cn->st.st_flags, STF_LOCK);
	}
	void SetTimeOut(int tmo)
	{
		cn->st.tmo = tmo;
		cn->st.tmo_left = tmo;
	}
	int GetTimeOut()
	{
		return cn->st.tmo;
	}
	bool ReadHup(void *arg, result_callback result)
	{
		return selectable_readhup(&cn->st, result, arg);
	}
	void RemoveReadHup()
	{
		selectable_remove_readhup(&cn->st);
	}
	void Shutdown()
	{
		selectable_shutdown(&cn->st);
	}
	int internal_read(WSABUF *buf, int bc)
	{
		return kfiber_net_readv(cn, buf, bc);
	}
	int internal_write(LPWSABUF buf, int bc)
	{
		return kfiber_net_writev(cn, buf, bc);
	}
	void AddSync()
	{
		selectable_add_sync(&cn->st);
	}
	void RemoveSync()
	{
		selectable_remove_sync(&cn->st);
	}
	kconnection *GetConnection()
	{
		return cn;
	}
	kev_result StartRequest();
	int end_request();
	bool internal_response_status(uint16_t status_code)
	{
		return false;
	}
	bool internal_response_header(const char *name, int name_len, const char *val, int val_len)
	{
		return false;
	}
	bool ResponseConnection(const char *val, int val_len)
	{
		return false;
	}
	int StartResponseBody(int64_t body_size)
	{
		return 0;
	}
	void SetDelay()
	{
		ksocket_delay(cn->st.fd);
	}
	void SetNoDelay(bool forever)
	{
		ksocket_no_delay(cn->st.fd, forever);
	}
	kconnection *cn;
};
#endif

