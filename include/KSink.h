#ifndef KSINK_H_99
#define KSINK_H_99
#include "kselector.h"
#include "kserver.h"
#include "kmalloc.h"
#include "KResponseContext.h"
#include "kgl_ssl.h"
#include "kstring.h"
#include "KRequest.h"

class KSink
{
public:
	KSink(kgl_pool_t* pool)
	{
		init_pool(pool);
	}
	virtual ~KSink()
	{
		if (pool) {
			kgl_destroy_pool(pool);
		}
		set_state(STATE_UNKNOW);
	}
	void push_flow_info(KFlowInfo* fi)
	{
		KFlowInfoHelper* helper = new KFlowInfoHelper(fi);
		helper->next = data.fh;
		data.fh = helper;
	}
	void add_up_flow(int flow)
	{
		KFlowInfoHelper* helper = data.fh;
		while (helper) {
			helper->fi->AddUpFlow((INT64)flow);
			helper = helper->next;
		}
	}
	bool adjust_range(int64_t* len);
	void add_down_flow(int flow, bool is_header_length = false)
	{
		if (!is_header_length) {
			data.send_size += flow;
		}
		KFlowInfoHelper* helper = data.fh;
		while (helper) {
			helper->fi->AddDownFlow((INT64)flow, KBIT_TEST(data.flags, RQ_CACHE_HIT));
			helper = helper->next;
		}
	}
	inline bool response_connection() {
#ifdef HTTP_PROXY
		if (data.connection_connect_proxy) {
			return false;
		}
#endif
		if (KBIT_TEST(data.flags, RQ_CONNECTION_UPGRADE)) {
			return ResponseConnection(kgl_expand_string("upgrade"));
		} else if (KBIT_TEST(data.flags, RQ_CONNECTION_CLOSE) || !KBIT_TEST(data.flags, RQ_HAS_KEEP_CONNECTION)) {
			return ResponseConnection(kgl_expand_string("close"));
		}
		if (data.http_major == 1 && data.http_minor >= 1) {
			//HTTP/1.1 default keep-alive
			return true;
		}
		return ResponseConnection(kgl_expand_string("keep-alive"));
	}
	bool response_content_length(int64_t content_len);

	inline bool response_status(uint16_t status_code)
	{
		if (data.status_code > 0) {
			//status_code只能发送一次
			return false;
		}
		set_state(STATE_SEND);
		data.first_response_time_msec = kgl_current_msec;
		data.status_code = status_code;
		return internal_response_status(status_code);
	}
	virtual bool read_hup(void* arg, result_callback result) = 0;
	virtual void RemoveReadHup() = 0;
	virtual bool SetTransferChunked() {
		return response_header(kgl_expand_string("Transfer-Encoding"), kgl_expand_string("chunked"));
	}
	virtual bool response_header(const char* name, int name_len, const char* val, int val_len) = 0;
	kgl_pool_t* GetConnectionPool()
	{
		return GetConnection()->pool;
	}
	virtual sockaddr_i* get_peer_addr()
	{
		kconnection* cn = GetConnection();
		return &cn->addr;
	}
	bool get_peer_ip(char* ips, int ips_len)
	{
		sockaddr_i* addr = get_peer_addr();
		return ksocket_sockaddr_ip(addr, ips, ips_len);
	}
	uint16_t get_self_port() {
		if (data.self_port > 0) {
			return data.self_port;
		}
		sockaddr_i addr;
		if (!get_self_addr(&addr)) {
			return 0;
		}
		data.self_port = ksocket_addr_port(&addr);
		return data.self_port;
	}
	void set_self_port(uint16_t port, bool ssl)
	{
		if (port > 0) {
			data.self_port = port;
		}
		if (ssl) {
			KBIT_SET(data.raw_url->flags, KGL_URL_SSL);
			if (data.raw_url->port == 80) {
				data.raw_url->port = 443;
			}
		} else {
			KBIT_CLR(data.raw_url->flags, KGL_URL_SSL);
			if (data.raw_url->port == 443) {
				data.raw_url->port = 80;
			}
		}
	}
	uint16_t get_self_ip(char* ips, int ips_len)
	{
		sockaddr_i addr;
		if (!get_self_addr(&addr)) {
			return 0;
		}
		if (!ksocket_sockaddr_ip(&addr, ips, ips_len)) {
			return 0;
		}
		if (data.self_port > 0) {
			return data.self_port;
		}
		data.self_port = ksocket_addr_port(&addr);
		return data.self_port;
	}
	const char* get_state();
	void set_state(uint8_t state);
	virtual void AddSync() = 0;
	virtual void RemoveSync() = 0;
	kselector* GetSelector()
	{
		return GetConnection()->st.selector;
	}
	virtual void Shutdown() = 0;
	kserver* GetBindServer()
	{
		return GetConnection()->server;
	}
	virtual kconnection* GetConnection() = 0;
	virtual void SetTimeOut(int tmo_count) = 0;

	virtual int GetTimeOut() = 0;
	virtual void SetDelay()
	{
	}
	virtual void SetNoDelay(bool forever)
	{
	}
	void Flush()
	{
		SetNoDelay(false);
		SetDelay();
	}
#ifdef KSOCKET_SSL
	kssl_session* GetSSL() {
		kconnection* cn = GetConnection();
		return cn->st.ssl;
	}
#endif
	const char* get_client_ip()
	{
		if (data.client_ip) {
			return data.client_ip;
		}
		data.client_ip = (char*)xmalloc(MAXIPLEN);
		if (get_peer_ip(data.client_ip, MAXIPLEN)) {
			return data.client_ip;
		}
		return "";
	}
	void set_client_ip(const char* ip)
	{
		if (data.client_ip) {
			xfree(data.client_ip);
		}
		data.client_ip = strdup(ip);
	}

	bool start_response_body(INT64 body_len);
	int write(WSABUF* buf, int bc);
	int write(const char* buf, int len);
	bool write(kbuf* buf);
	int read(char* buf, int len);
	bool write_all(const char* buf, int len);
	bool parse_header(const char* attr, int attr_len, char* val, int val_len, bool is_first);
	void begin_request();
	virtual int end_request() = 0;
	void set_if_none_match(const char* etag, int len)
	{
		data.if_none_match = (kgl_str_t*)kgl_pnalloc(pool, sizeof(kgl_str_t));
		data.if_none_match->data = (char*)kgl_pnalloc(pool, len + 1);
		data.if_none_match->len = len;
		kgl_memcpy(data.if_none_match->data, etag, len + 1);
	}
	void clean_if_none_match()
	{
		data.if_none_match = NULL;
	}
	virtual bool get_self_addr(sockaddr_i* addr)
	{
		return 0 == kconnection_self_addr(GetConnection(), addr);
	}
	kgl_pool_t* pool;
	KRequestData data;
	friend class KHttp2;
protected:
	virtual void start_header()
	{

	}
	void start_parse();
	void reset_pipeline();
	kgl_header_result internal_parse_header(const char* attr, int attr_len, char* val, int* val_len, bool is_first);
	void init_pool(kgl_pool_t* pool);
	virtual int internal_write(WSABUF* buf, int bc) = 0;
	virtual int internal_read(char* buf, int len) = 0;
	virtual bool internal_response_status(uint16_t status_code) = 0;
	virtual bool ResponseConnection(const char* val, int val_len) = 0;
	virtual int StartResponseBody(int64_t body_size) = 0;
};
#endif
