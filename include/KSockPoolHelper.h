#ifndef KSOCKPOOLHELPER_H_
#define KSOCKPOOLHELPER_H_
#include <map>
#include <string>
#include <list>
#include "ksocket.h"
#include "KAtomCountable.h"
#include "KPoolableSocketContainer.h"
#include "KTcpUpstream.h"

#define KSOCKET_FLAGS_SKIP_POOL 1
#define KSOCKET_FLAGS_WEBSOCKET (1<<1)

#define ERROR_RECONNECT_TIME	600
class KSockPoolHelper : public KPoolableSocketContainer {
public:
	KSockPoolHelper();
	virtual ~KSockPoolHelper();
	void health(KUpstream *st,HealthStatus stage) override
	{
		katom_inc64((void *)&this->total_error);
		switch(stage){
		case HealthStatus::Err:
			error_count++;
			if (error_count>=max_error_count) {
				disable();
			}
			break;
		case HealthStatus::Success:
			enable();
			break;
		default:
			break;
		}		
	}
	bool isChanged(KSockPoolHelper *sh)
	{
		if (host!=sh->host) {
			return true;
		}
		if (port!=sh->port) {
			return true;
		}
#ifdef ENABLE_UPSTREAM_SSL
		if (ssl!=sh->ssl) {
			return true;
		}
		if (no_sni != sh->no_sni) {
			return true;
		}
#endif
		if (weight!=sh->weight) {
			return true;
		}
		if (is_unix!=sh->is_unix) {
			return true;
		}
		if (ip!=sh->ip || (ip && strcmp(ip,sh->ip)!=0)) {
			return true;
		}
		if (lifeTime != sh->lifeTime) {
			return true;
		}
		if (sign != sh->sign) {
			return true;
		}
		return false;
	}
	void start_monitor_call_back();
	//void syncCheckConnect();
	void setErrorTryTime(int max_error_count, int error_try_time)
	{
		lock.Lock();
		this->max_error_count = max_error_count;
		this->error_try_time = error_try_time;
		if (max_error_count > 0) {
			internalStopMonitor();
		} else {
			startMonitor();
		}
		lock.Unlock();
	}
	bool parse(std::map<std::string, std::string>& attr);
	void build(std::map<std::string, std::string>& attr);
	KUpstream* get_upstream(uint32_t flags ,const char *sni_host = NULL);
	void checkActive();
	bool setHostPort(std::string host,int port,const char *ssl);
	bool setHostPort(std::string host, const char *port);
	void disable();
	void enable();
	bool isEnable();
	void setIp(const char *ip)
	{
		if (this->ip) {
			free(this->ip);
			this->ip = NULL;
		}
		if (ip && *ip) {
			this->ip = strdup(ip);
		}
	}
	const char *getIp()
	{
		return this->ip;
	}
	std::string host;
	uint64_t hit;
	uint16_t weight;
	uint16_t port;
	union {
		struct {
			uint32_t monitor : 1;
			uint32_t is_unix : 1;
			uint32_t sign : 1;
#ifdef ENABLE_UPSTREAM_SSL
			uint32_t no_sni : 1;
#endif
		};
		uint32_t flags;
	};
#ifdef ENABLE_UPSTREAM_SSL
#ifdef ENABLE_UPSTREAM_HTTP2
	u_char alpn;
#endif
	bool IsSslVerify()
	{
		return ssl == "S";
	}
	std::string ssl;
#endif
	int error_try_time;
	/*
	 * �����������Ӵ������������MAX_ERROR_COUNT�Σ��ͻ���Ϊ������ġ�
	 * �´�������ʱ���ӵ�ǰʱ���ERROR_RECONNECT_TIME�롣
	 */
	uint16_t error_count;
	uint16_t max_error_count;
	/*
	 * �´�������ʱ�䣬�����0��ʾ��Ծ�ġ�
	 */
	time_t tryTime;
	void monitorNextTick();
	void stopMonitor()
	{
		lock.Lock();
		internalStopMonitor();
		lock.Unlock();
	}
	volatile uint64_t total_error;
	volatile uint64_t total_connect;
	INT64 monitor_start_time;
	int avg_monitor_tick;
	KSockPoolHelper *next;
	KSockPoolHelper *prev;
private:
	void startMonitor();
	void internalStopMonitor()
	{
		monitor = 0;
	}
	char *ip;
	KMutex lock;
#ifdef ENABLE_UPSTREAM_SSL
	SSL_CTX *ssl_ctx;
#endif
};

#endif /* KSOCKPOOLHELPER_H_ */
