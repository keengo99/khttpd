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


class KPoolableSocketContainer;
class KWriteStream;
class KUpstream;
typedef bool (*kgl_header_callback)(KUpstream* us, void* arg, const char* attr, int attr_len, const char* val, int val_len,bool is_first);

class KUpstreamCallBack
{
public:
	KUpstreamCallBack()
	{
		memset(this, 0, sizeof(KUpstreamCallBack));
	}
	kgl_header_callback header;
	void* arg;
};

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
	KGL_RESULT write_all(const char* buf, int len)
	{
		while (len > 0) {
			WSABUF bufs;
			bufs.iov_base = (char *)buf;
			bufs.iov_len = len;
			int this_len = write(&bufs, 1);
			if (this_len <= 0) {
				return KGL_EIO;
			}
			len -= this_len;
			buf += this_len;
		}
		return KGL_OK;
	}
	virtual bool send_connection(const char* val, hlen_t val_len) = 0;
	virtual bool send_header(const char* attr, hlen_t attr_len, const char* val, hlen_t val_len) = 0;
	virtual bool send_method_path(uint16_t meth, const char* path, hlen_t path_len) = 0;
	virtual bool send_host(const char* host, hlen_t host_len) = 0;
	virtual void set_content_length(int64_t content_length) = 0;
	virtual KGL_RESULT send_header_complete() = 0;
	virtual void BindOpaque(KOPAQUE data) = 0;
	virtual bool set_header_callback(void* arg, kgl_header_callback header_callback) = 0;
	virtual KGL_RESULT read_header() = 0;
	virtual kgl_pool_t* GetPool() = 0;
	virtual kconnection *GetConnection() = 0;
	virtual void write_end()
	{

	}
	virtual int read(char *buf, int len) = 0;
	virtual int write(WSABUF *buf, int bc) = 0;
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
	virtual kgl_refs_string* get_param();
	virtual void gc(int life_time) = 0;
	friend class KPoolableSocketContainer;
	union
	{
		time_t expire_time;
		//记录开始读的时间，用于长连接计算超时用的。
		time_t read_header_time;
	};
	KPoolableSocketContainer *container;
protected:
	virtual void clean()
	{

	}
	virtual ~KUpstream();
};
#endif
