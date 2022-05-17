/*
 * KPoolableStreamContainer.h
 *
 *  Created on: 2010-8-18
 *      Author: keengo
 */

#ifndef KPOOLABLESTREAMCONTAINER_H_
#define KPOOLABLESTREAMCONTAINER_H_
#include <list>
#include "KUpstream.h"
#include "KStringBuf.h"
#include "KMutex.h"
#include "KAtomCountable.h"
void SafeDestroyUpstream(KUpstream *st);
/*
 * 连接池容器类
 */
class KPoolableSocketContainerImp {
public:
	KPoolableSocketContainerImp();
	~KPoolableSocketContainerImp();
	void refresh(bool clean);
	void refreshList(kgl_list *l, bool clean);
	kgl_list *GetList()
	{
		return &head;
	}	
	unsigned size;
protected:
	kgl_list head;
};
class KPoolableSocketContainer: public KAtomCountable {
public:
	KPoolableSocketContainer();
	virtual ~KPoolableSocketContainer();

	/*
	回收连接
	close,是否关闭
	lifeTime 连接时间
	*/
	virtual void gcSocket(KUpstream *st,int lifeTime);
	void bind(KUpstream *st);
	void unbind(KUpstream *st);
	int getLifeTime() {
		return lifeTime;
	}
	/*
	 * 设置连接超时时间
	 */
	void setLifeTime(int lifeTime);
	/*
	 * 定期刷新删除过期连接
	 */
	virtual void refresh(time_t nowTime);
	/*
	 * 清除所有连接
	 */
	void clean();
	/*
	 * 得到连接数
	 */
	 unsigned getSize() {
		unsigned size = 0;
		lock.Lock();
		if (imp) {
			size = imp->size;
		}
		lock.Unlock();
		return size;
	}
	//isBad,isGood用于监控连接情况
	virtual void isBad(KUpstream *st,BadStage stage)
	{
	}
	virtual void isGood(KUpstream *st)
	{
	}
	kgl_refs_string* GetParam();
	void SetParam(const char* param);
	virtual void set_tcp(bool tcp)
	{
		this->tcp = tcp;
	}
#ifdef HTTP_PROXY
	virtual void addHeader(KHttpRequest *rq,KHttpEnv *s)
	{
	}
#endif
protected:
	KUpstream* new_upstream(kconnection* cn);
	KUpstream* get_pool_socket();
	/*
	 * 把连接真正放入池中
	 */
	void PutPoolSocket(KUpstream *st);
	union
	{
		struct
		{
			uint16_t tcp : 1;
			uint16_t lifeTime;
		};
		uint32_t flags;
	};
	kgl_refs_string* param;
	KMutex lock;
private:
	
	KUpstream *internalGetPoolSocket();
	time_t getHttp2ExpireTime()
	{
		int lifeTime = this->lifeTime;
		if (lifeTime <= 10) {
			//http2最少10秒连接时间
			lifeTime = 10;
		}
		return kgl_current_sec + lifeTime;
	}
	KPoolableSocketContainerImp *imp;
};
#endif /* KPOOLABLESTREAMCONTAINER_H_ */
