/*
 * KPoolableStream.h
 *
 *  Created on: 2010-8-18
 *      Author: keengo
 */

#ifndef KUPSTREAM_H
#define KUPSTREAM_H
#include <time.h>
#include "kselector.h"
#include "ksocket.h"
#include "kconnection.h"
#include "kmalloc.h"
#include "kstring.h"
#include "KHttpHeader.h"
enum BadStage
{
	/* BadStage_Connect BadStage_TrySend  */
	BadStage_Connect,
	BadStage_TrySend,
	/* BadStage_SendSuccess  */
	BadStage_SendSuccess,
};

class KUpstreamCallBack {
public:
	KUpstreamCallBack()
	{
		memset(this, 0, sizeof(KUpstreamCallBack));
	}
	kgl_header_callback header;
	void* arg;
};
class KPoolableSocketContainer;
class KWriteStream;

class KUpstream
{
public:
	KUpstream()
	{
		expire_time = 0;
		container = NULL;
	}
	virtual void SetDelay()
	{

	}
	virtual void SetNoDelay(bool forever)
	{
	}
	virtual KOPAQUE GetOpaque()
	{
		return GetConnection()->st.data;
	}
	kselector* GetSelector()
	{
		return GetConnection()->st.selector;
	}
	virtual bool send_header(const char* attr, hlen_t attr_len, const char* val, hlen_t val_len) = 0;
	virtual bool send_method_path(uint16_t meth, const char* path, hlen_t path_len) = 0;
	virtual bool send_host(const char* host, hlen_t host_len) = 0;
	virtual bool send_content_length(int64_t content_length) = 0;
	virtual bool send_header_complete(int64_t post_body_len) = 0;
	virtual void BindOpaque(KOPAQUE data) = 0;
	virtual bool set_header_callback(void* arg, kgl_header_callback header_callback) = 0;
	virtual KGL_RESULT read_header() = 0;
	virtual kgl_pool_t *GetPool()
	{
		return NULL;
	}
	virtual kconnection *GetConnection() = 0;
	virtual void WriteEnd()
	{

	}
	virtual int Read(char* buf, int len) = 0;
	virtual int Write(WSABUF * buf, int bc) = 0;
	virtual void Shutdown() = 0;
	virtual void Destroy() = 0;
	virtual bool IsMultiStream()
	{
		return false;
	}
	virtual void SetTimeOut(int tmo)
	{
	}
	virtual void BindSelector(kselector *selector)
	{
	}
	virtual void OnPushContainer()
	{
	}
	virtual KUpstream *NewStream()
	{
		return NULL;
	}
	virtual bool IsNew() {
		return expire_time == 0;
	}
	virtual int GetLifeTime();
	virtual void IsGood();	
	virtual void IsBad(BadStage stage);
	virtual sockaddr_i *GetAddr() = 0;
	bool GetSelfAddr(sockaddr_i *addr)
	{
		kconnection *cn = GetConnection();
		if (cn == NULL) {
			return false;
		}
		return 0 == kconnection_self_addr(cn, addr);
	}
	uint16_t GetSelfPort()
	{
		sockaddr_i addr;
		if (!GetSelfAddr(&addr)) {
			return 0;
		}
		return ksocket_addr_port(&addr);
	}
	virtual void gc(int life_time,time_t base_time) = 0;
	friend class KPoolableSocketContainer;
	time_t expire_time;
	KPoolableSocketContainer *container;
protected:
	virtual ~KUpstream();
};
#define KPoolableUpstream KUpstream
#endif /* KPOOLABLESTREAM_H_ */
