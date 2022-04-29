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

class KRequestData: public KHttpHeaderManager {
public:
	~KRequestData();
	KRequestData() {
		memset(this, 0, sizeof(*this));
		begin_time_msec = kgl_current_msec;
	}
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
	uint32_t flags;
	uint8_t http_major : 4;
	uint8_t http_minor : 4;
	uint8_t meth;
	//response
	uint16_t cache_hit : 1;
	uint16_t connection_upgrade : 1;
	uint16_t body_not_complete : 1;
	uint16_t header_has_send : 1;
	uint16_t te_chunked : 1;//transfer-encoding: chunked
	uint16_t te_compressed : 1;
	uint16_t status_code;
	int64_t first_response_time_msec;
	/*
	 * 原始url
	 */
	KUrl raw_url;
	KUrl* url;
	friend class KSink;
	void pushFlowInfo(KFlowInfo* fi)
	{
		KFlowInfoHelper* helper = new KFlowInfoHelper(fi);
		helper->next = fh;
		fh = helper;
	}
	//这个内存由KSink的pool自动管理
	kgl_str_t* if_none_match;
private:
	bool parse_method(const char* src);
	bool parse_connect_url(char* src);
	bool parse_http_version(char* ver);
	kgl_header_result parse_host(char* val);
	void clean();
	void init();
	//流量统计
	KFlowInfoHelper* fh;
	char* client_ip;
};
#endif
