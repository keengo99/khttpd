#ifndef KREQUEST_H_99ddd
#define KREQUEST_H_99ddd
#include "kfeature.h"
#include "KFlowInfo.h"
#include "khttp.h"
#include "kmalloc.h"
#include "ksocket.h"
#include "KUrl.h"
#include "KHttpHeaderManager.h"
#include "KHttpParser.h"
#include "KHttpKeyValue.h"
#include "KHttpOpaque.h"

#define STATE_IDLE     0
#define STATE_SEND     1
#define STATE_RECV     2
#define STATE_WAIT     3

extern volatile uint64_t kgl_total_requests;
extern volatile uint64_t kgl_total_accepts;
extern volatile uint64_t kgl_total_servers;
extern volatile uint32_t kgl_reading;
extern volatile uint32_t kgl_writing;
extern volatile uint32_t kgl_waiting;

class KRequestPlainData
{
public:
		int64_t send_size;
		//post数据还剩多少数据没处理
		int64_t left_read;
		time_t min_obj_verified;
		kgl_request_range* range;
		kgl_precondition precondition;
		int64_t begin_time_msec;
		int64_t first_response_time_msec;
		uint32_t flags;
		uint16_t status_code;
		uint16_t self_port;
};
class KRequestData: public KHttpHeaderManager ,public KRequestPlainData {
public:
	~KRequestData();
	KRequestData() {
		memset(this, 0, sizeof(*this));
		begin_time_msec = kgl_current_msec;
	}
	uint32_t mark;
	uint16_t http_version;
	uint8_t state;
	uint8_t meth;
	/*
	 * 原始url
	 */
	KUrl *raw_url;
	KUrl *url;
	KHttpOpaque* opaque;

	friend class KSink;
	void set_http_version(uint8_t major, uint8_t minor) {
		http_version = ((major << 8) | minor);
	}
	uint8_t get_http_version_major() {
		return http_version >> 8;
	}
	uint8_t get_http_version_minor() {
		return (http_version & 0xff);
	}
	void bind_opaque(KHttpOpaque* opaque)
	{
		if (this->opaque) {
			this->opaque->release();
		}
		this->opaque = opaque;
	}
	char* client_ip;
private:
	void start_parse();
	void free_lazy_memory();
	bool parse_method(const char* src,int len);
	bool parse_connect_url(u_char* src, size_t len);
	bool parse_http_version(u_char* ver, size_t len);
	bool parse_host(const char* val,size_t len);
	void init();
	void clean();
	KFlowInfoHelper* fh;
};
#endif
