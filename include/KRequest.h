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

#define STATE_UNKNOW   0
#define STATE_IDLE     1
#define STATE_SEND     2
#define STATE_RECV     3
#define STATE_QUEUE    4

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
		//post数据长度
		int64_t content_length;
		time_t if_modified_since;
		time_t min_obj_verified;
		int64_t range_from;
		int64_t range_to;
		int64_t begin_time_msec;
		int64_t first_response_time_msec;
		//这个内存由KSink的pool自动管理
		kgl_str_t* if_none_match;
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
	uint8_t http_major;
	uint8_t http_minor;
	uint8_t state;
	uint8_t meth;
	uint32_t mark;
	/*
	 * 原始url
	 */
	KUrl *raw_url;
	KUrl *url;
	KHttpOpaque* opaque;

	friend class KSink;
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
	kgl_header_result parse_host(char* val,size_t len);
	void init();
	void clean();
	KFlowInfoHelper* fh;
};
#endif
