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
 * ���ӳ�������
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
	��������
	close,�Ƿ�ر�
	lifeTime ����ʱ��
	*/
	virtual void gcSocket(KUpstream *st,int lifeTime);
	void bind(KUpstream *st);
	void unbind(KUpstream *st);
	int getLifeTime() {
		return lifeTime;
	}
	/*
	 * �������ӳ�ʱʱ��
	 */
	void setLifeTime(int lifeTime);
	/*
	 * ����ˢ��ɾ����������
	 */
	virtual void refresh(time_t nowTime);
	/*
	 * �����������
	 */
	void clean();
	/*
	 * �õ�������
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
	//isBad,isGood���ڼ���������
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
	 * �����������������
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
			//http2����10������ʱ��
			lifeTime = 10;
		}
		return kgl_current_sec + lifeTime;
	}
	KPoolableSocketContainerImp *imp;
};
#endif /* KPOOLABLESTREAMCONTAINER_H_ */
