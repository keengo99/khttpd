#ifndef KSINK_H_99
#define KSINK_H_99
#include "kselector.h"
#include "kserver.h"
#include "kmalloc.h"
#include "KResponseContext.h"
#include "kgl_ssl.h"
#include "kstring.h"
#include "KRequest.h"
#include "kfiber.h"
#define KGL_FLAG_PRECONDITION_MASK  7
class KSink
{
public:
	KSink(kgl_pool_t* pool);
	virtual ~KSink();

	template<typename T>
	T* alloc() {
		T* t = (T*)kgl_pnalloc(pool, sizeof(T));
		memset(t, 0, sizeof(T));
		return t;
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
	//called by low level to start sink.
	virtual kev_result read_header() = 0;
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
		if (data.meth==METH_CONNECT) {
			return false;
		}
#endif
		if (KBIT_TEST(data.flags, RQ_CONNECTION_UPGRADE)) {
			return response_connection(kgl_expand_string("upgrade"));
		} else if (KBIT_TEST(data.flags, RQ_CONNECTION_CLOSE) || !KBIT_TEST(data.flags, RQ_HAS_KEEP_CONNECTION)) {
			return response_connection(kgl_expand_string("close"));
		}
		if (data.http_version > 0x100) {
			//HTTP/1.1 default keep-alive
			return true;
		}
		return response_connection(kgl_expand_string("keep-alive"));
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
	virtual bool response_header(kgl_header_type know_header,const char *val, int val_len, bool lock_value)
	{
		assert(know_header<kgl_header_unknow);
		return response_header(kgl_header_type_string[know_header].value.data, (int)kgl_header_type_string[know_header].value.len, val, val_len);
	}
	virtual bool readhup(void* arg, result_callback result) = 0;
	virtual void remove_readhup() = 0;
	virtual bool set_transfer_chunked() {
		return response_header(kgl_header_transfer_encoding, _KS("chunked"), true);
	}
	virtual bool response_trailer(const char* name, int name_len, const char* val, int val_len) {
		return true;
	}
	virtual bool response_header(const char* name, int name_len, const char* val, int val_len) = 0;
	virtual sockaddr_i* get_peer_addr() = 0;
	bool get_peer_ip(char* ips, int ips_len)
	{
		sockaddr_i* addr = get_peer_addr();
		if (addr == nullptr) {
			return false;
		}
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
	virtual kselector* get_selector() = 0;
	virtual void shutdown() = 0;
	virtual uint32_t get_server_model() = 0;
	virtual KOPAQUE get_server_opaque() = 0;
	virtual KHttpHeader* get_trailer() {
		return nullptr;
	}
	virtual void set_time_out(int tmo_count) = 0;
	virtual kgl_pool_t* get_connection_pool() = 0;
	virtual int get_time_out() = 0;
	virtual void set_delay()
	{
	}
	virtual void set_no_delay(bool forever)
	{
	}
	virtual void flush()
	{
		set_no_delay(false);
		set_delay();
	}
	virtual kssl_session* get_ssl() {
		return nullptr;
	}
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
	int read(char* buf, int len);
	bool write_all(const char* buf, int len);
	bool parse_header(const char* attr, int attr_len, const char* val, int val_len, bool is_first);
	bool begin_request();
	virtual int end_request() = 0;
	virtual bool is_locked() = 0;
	kgl_precondition* get_precondition(kgl_precondition_flag* flag) {
		*flag = (kgl_precondition_flag)(data.flags & KGL_FLAG_PRECONDITION_MASK);
		return data.precondition;
	}
	virtual kgl_proxy_protocol* get_proxy_info()
	{
		return nullptr;
	}
	virtual void* get_sni()
	{
		return nullptr;
	}
	virtual bool support_sendfile() {
		return false;
	}
	virtual int sendfile(kasync_file* fp, int len) {
		return -1;
	}
	virtual bool get_self_addr(sockaddr_i* addr) = 0;
	kgl_str_t* alloc_entity(const char* entity_value, int len) {
		kgl_str_t* entity = alloc<kgl_str_t>();
		entity->data = (char*)kgl_pnalloc(pool, len + 1);
		entity->len = len;
		kgl_memcpy(entity->data, entity_value, len);
		entity->data[len] = '\0';
		return entity;
	}
	kgl_list queue;
	kgl_pool_t* pool;
	KRequestData data;
	friend class KHttp2;
protected:
	void start_parse();
	void reset_pipeline();
	void init_pool(kgl_pool_t* pool);
	virtual int internal_write(WSABUF* buf, int bc) = 0;
	virtual int internal_read(char* buf, int len) = 0;
	virtual bool internal_response_status(uint16_t status_code) = 0;
	virtual bool response_connection(const char* val, int val_len) = 0;
	virtual int internal_start_response_body(int64_t body_size, bool is_100_continue) = 0;
private:
	bool response_100_continue();

	kgl_request_range* alloc_request_range() {
		if (data.range) {
			return data.range;
		}
		data.range = (kgl_request_range*)kgl_pnalloc(pool, sizeof(kgl_request_range));
		*data.range = { 0 };
		return data.range;
	}
	kgl_precondition* alloc_precondition() {
		if (data.precondition) {
			return data.precondition;
		}
		data.precondition = (kgl_precondition *)kgl_pnalloc(pool, sizeof(kgl_precondition));
		*data.precondition = { 0 };
		return data.precondition;
	}
};
bool kgl_init_sink_queue();
typedef bool (*kgl_sink_iterator)(void* ctx, KSink* sink);
void kgl_iterator_sink(kgl_sink_iterator it, void* ctx);
#endif
