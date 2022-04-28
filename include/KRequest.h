#ifndef KREQUEST_H_99ddd
#define KREQUEST_H_99ddd
#include "kfeature.h"
#include "KFlowInfo.h"
#include "khttp.h"
#include "kmalloc.h"
#include "ksocket.h"
#include "KSink.h"
#include "KUrl.h"
#include "KHttpHeaderManager.h"
#include "KHttpParser.h"
#include "KHttpKeyValue.h"


class KHttpRequestData {
public:
	int64_t send_size;
	uint32_t flags;	
	uint8_t http_major : 4;
	uint8_t http_minor : 4;
	uint8_t meth;
	//post数据还剩多少数据没处理
	int64_t left_read;
	//post数据长度
	int64_t content_length;
	time_t if_modified_since;
	time_t min_obj_verified;
	int64_t range_from;
	int64_t range_to;
	int64_t begin_time_msec;
};
class KHttpResponseData {
public:
	int64_t first_response_time_msec;
	uint16_t status_code;
	uint32_t cache_hit : 1;
	uint32_t connection_upgrade : 1;
	uint32_t body_not_complete : 1;
	uint32_t header_has_send : 1;
	uint32_t te_chunked : 1;//transfer-encoding: chunked
};
class KRequest: public KHttpHeaderManager {
public:
	~KRequest();
	KRequest(KSink* sink, kgl_pool_t* pool) {
		memset(this, 0, sizeof(*this));
		this->sink = sink;
		InitPool(pool);
		req.begin_time_msec = kgl_current_msec;
	}
	inline bool response_status(uint16_t status_code)
	{
		if (res.status_code > 0) {
			//status_code只能发送一次
			return false;
		}
		res.first_response_time_msec = kgl_current_msec;
		res.status_code = status_code;
		return sink->ResponseStatus(this, status_code);
	}
	inline bool response_header(kgl_header_type name, const char* val, hlen_t val_len)
	{
		return this->response_header(kgl_header_type_string[name].data, (hlen_t)kgl_header_type_string[name].len, val, val_len);
	}
	inline bool response_header(KHttpHeader* header)
	{
		return response_header(header->attr, header->attr_len, header->val, header->val_len);
	}
	inline bool response_header(kgl_str_t* name, kgl_str_t* val)
	{
		return response_header(name->data, hlen_t(name->len), val->data, hlen_t(val->len));
	}
	inline bool responseHeader(kgl_str_t* name, const char* val, hlen_t val_len)
	{
		return response_header(name->data, hlen_t(name->len), val, hlen_t(val_len));
	}
	inline bool response_header(const char* name, hlen_t name_len, int val)
	{
		char buf[16];
		int len = snprintf(buf, sizeof(buf) - 1, "%d", val);
		return response_header(name, name_len, buf, len);
	}
	bool response_content_length(int64_t content_length);
	//返回true，一定需要回应content-length或chunk
	inline bool response_connection() {
#ifdef HTTP_PROXY
		if (ctx->connection_connect_proxy) {
			return false;
		}
#endif
		if (res.connection_upgrade) {
			return sink->ResponseConnection(kgl_expand_string("upgrade"));
		}
		else if (KBIT_TEST(req.flags, RQ_CONNECTION_CLOSE) || !KBIT_TEST(req.flags, RQ_HAS_KEEP_CONNECTION)) {
			return sink->ResponseConnection(kgl_expand_string("close"));
		}
		if (req.http_major == 1 && req.http_minor >= 1) {
			//HTTP/1.1 default keep-alive
			return true;
		}
		return sink->ResponseConnection(kgl_expand_string("keep-alive"));
	}
	bool response_header(const char* name, hlen_t name_len, const char* val, hlen_t val_len)
	{
		return sink->ResponseHeader(name, name_len, val, val_len);
	}
	//发送完header开始发送body时调用
	bool start_response_body(INT64 body_len);

/*
 * 原始url
 */
	KUrl raw_url;
	KUrl* url;
	friend class KSink;
	friend class KHttpSink;
	friend class KHttp2Sink;
	friend class KTcpSink;
	friend class KHttp2;

	void pushFlowInfo(KFlowInfo* fi)
	{
		KFlowInfoHelper* helper = new KFlowInfoHelper(fi);
		helper->next = fh;
		fh = helper;
	}
	kgl_str_t* if_none_match;
	KHttpRequestData req;
	KHttpResponseData res;
	const char* get_client_ip()
	{
		if (client_ip) {
			return client_ip;
		}
		client_ip = (char*)xmalloc(MAXIPLEN);
		sockaddr_i* addr = sink->GetAddr();
		ksocket_sockaddr_ip(addr, client_ip, MAXIPLEN);
		return client_ip;
	}
	void set_client_ip(const char* ip)
	{
		if (client_ip) {
			xfree(client_ip);
		}
		client_ip = strdup(ip);
	}
	int write(WSABUF* buf, int bc);
	int write(const char* buf, int len);
	bool write(kbuf* buf);
	bool write_full(const char* buf, int len);
	int end_request();
	bool parse_header(const char* attr, int attr_len, char* val, int val_len, bool is_first);
	KSink* sink;
	kgl_pool_t* pool;
private:

	void add_up_flow(int flow)
	{
		KFlowInfoHelper* helper = fh;
		while (helper) {
			helper->fi->AddUpFlow((INT64)flow);
			helper = helper->next;
		}
	}
	void add_down_flow(int flow, bool is_header_length = false)
	{
		if (!is_header_length) {
			req.send_size += flow;
		}
		KFlowInfoHelper* helper = fh;
		while (helper) {
			helper->fi->AddDownFlow((INT64)flow, res.cache_hit);
			helper = helper->next;
		}
	}
	void set_if_none_match(const char* etag, int len)
	{

		if_none_match = (kgl_str_t*)kgl_pnalloc(pool,sizeof(kgl_str_t));
		if_none_match->data = (char*)kgl_pnalloc(pool, len + 1);
		if_none_match->len = len;
		kgl_memcpy(if_none_match->data, etag, len + 1);
	}
	void start_parse();
	kgl_header_result InternalParseHeader(const char* attr, int attr_len, char* val, int* val_len, bool is_first);
	bool parse_method(const char* src);
	bool parse_connect_url(char* src);
	bool parse_http_version(char* ver);
	kgl_header_result parse_host(char* val);
	void clean();
	void init(kgl_pool_t* pool);
	void FreeLazyMemory();
	//流量统计
	KFlowInfoHelper* fh;
	char* client_ip;
	void InitPool(kgl_pool_t* pool) {
		kassert(this->pool == NULL);
		this->pool = pool;
		if (this->pool == NULL) {
			this->pool = kgl_create_pool(KGL_REQUEST_POOL_SIZE);
		}
	}

};
#endif
