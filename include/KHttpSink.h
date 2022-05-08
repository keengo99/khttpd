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
	
	bool response_header(const char *name, int name_len, const char *val, int val_len);
	bool ResponseConnection(const char *val, int val_len) {
		return response_header(kgl_expand_string("Connection"), val, val_len);
	}
	void AddTimer(result_callback result, void *arg, int msec)
	{
		kselector_add_timer(cn->st.selector, result, arg, msec, &cn->st);
	}

	int StartResponseBody(int64_t body_size);
	bool IsLocked();
	int internal_read(char *buf, int len) override;
	int internal_write(WSABUF *buf, int bc) override;
	bool read_hup(void *arg, result_callback result)
	{
		return selectable_readhup(&cn->st, result, arg);
	}
	void RemoveReadHup()
	{
		selectable_remove_readhup(&cn->st);
	}
	void AddSync()
	{
		selectable_add_sync(&cn->st);
	}
	void RemoveSync()
	{
		selectable_remove_sync(&cn->st);
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
	int GetTimeOut()
	{
		return cn->st.tmo;
	}
	int end_request();
	KOPAQUE GetOpaque()
	{
		return cn->st.data;
	}
	ks_buffer buffer;
	kev_result ReadHeader();
	kev_result Parse();
	KResponseContext *rc;
	kconnection *cn;
	kev_result ResultResponseContext(int got);
	kconnection *GetConnection()
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
	void start_header();
	//@overide
	bool internal_response_status(uint16_t status_code);
	KDechunkContext *dechunk;
	khttp_parser parser;
};
#endif

