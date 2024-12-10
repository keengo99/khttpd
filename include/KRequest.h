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
	//post���ݻ�ʣ��������û����
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
class KRequestData : public KHttpHeaderManager, public KRequestPlainData {
public:
	~KRequestData() {
		clean();
		free_lazy_memory();
		if (opaque) {
			opaque->release();
		}
		assert(!header);
	}
	KRequestData(): raw_url(false){
		memset(this, 0, sizeof(*this));
		begin_time_msec = kgl_current_msec;
		begin_request();
	}
	uint32_t mark;
	uint16_t http_version;
	uint8_t state;
	uint8_t meth;
	/*
	 * ԭʼurl
	 */
	KUrl raw_url;
	KUrl* url;
	KHttpOpaque* opaque;
	friend class KSink;
	friend class KHttpSink;
	void set_http_version(uint8_t major, uint8_t minor) {
		http_version = ((major << 8) | minor);
	}
	uint8_t get_http_version_major() {
		return http_version >> 8;
	}
	uint8_t get_http_version_minor() {
		return (http_version & 0xff);
	}
	void bind_opaque(KHttpOpaque* opaque) {
		if (this->opaque) {
			this->opaque->release();
		}
		this->opaque = opaque;
	}
	/* client_ip use heap memory, In future will change to use pool memory. */
	char* client_ip;
protected:
	/* call begin_request when read first package or first new request */
	void begin_request() {
		meth = METH_UNSET;
		mark = 0;
		assert(url == NULL);
		assert(!raw_url.host);
	}

	void free_lazy_memory() {
		if (client_ip) {
			xfree(client_ip);
			client_ip = NULL;
		}
		raw_url.clean();
		mark = 0;
	}
	void free_header() {
		free_header_list(header);
		header = last = NULL;
	}
	bool parse_method(const char* src, int len) {
		meth = KHttpKeyValue::get_method(src, len);
		return meth >= 0;
	}
	bool parse_connect_url(u_char* src, size_t len) {
		u_char* ss = (u_char*)memchr(src, ':', len);
		if (!ss) {
			return false;
		}
		assert(!raw_url.host);
		KBIT_CLR(raw_url.flags, KGL_URL_ORIG_SSL);
		KBIT_SET(raw_url.flags, KGL_URL_HAS_PORT);
		raw_url.host = kgl_strndup((char*)src, ss - src);
		len -= (ss - src);
		raw_url.port = (uint16_t)kgl_atoi(ss + 1, len - 1);
		return true;
	}
	bool parse_host(const char* val, size_t len) {
		if (!raw_url.host) {
			return parse_url_host(&raw_url, val, len);
		}
		return true;
	}
	bool parse_http_version(u_char* ver, size_t len) {
		u_char* dot = (u_char*)memchr(ver, '.', len);
		if (dot == NULL) {
			return false;
		}
		if ((size_t)(dot - ver) < len) {
			set_http_version(!!(*(dot - 1) - 0x30), !!(*(dot + 1) - 0x30));
		} else {
			set_http_version(!!(*(dot - 1) - 0x30), 0);
		}
		return true;
	}
	/* call clean when end request */
	void clean() {
		if (url) {
			url->release();
			url = NULL;
		}
		while (fh) {
			KFlowInfoHelper* fh_next = fh->next;
			delete fh;
			fh = fh_next;
		}
	}
	/* call init when new request */
	void init() {
		KRequestPlainData* data = static_cast<KRequestPlainData*>(this);
		memset(data, 0, sizeof(KRequestPlainData));
		begin_time_msec = kgl_current_msec;
	}
	KFlowInfoHelper* fh;
};
#endif
