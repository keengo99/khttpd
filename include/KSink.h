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
//#define KGL_DEBUG_TIME
#ifdef KGL_DEBUG_TIME
void kgl_log_total_timespec() ;
#endif
class KSink
{
public:
#ifdef KGL_DEBUG_TIME
	void log_passed_time(const char *tip);
	void reset_start_time() {
		clock_gettime(CLOCK_REALTIME, &start_time);
	}
	struct timespec start_time;
#endif
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
	//virtual kev_result read_header() = 0;
	virtual void start(int header_len) = 0;
	bool adjust_range(int64_t* len) {
		assert(data.range);
		return kgl_adjust_range(data.range, len);
	}
	void add_down_flow(int flow)
	{
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
	bool response_header(kgl_header_type attr, int64_t value) {
		char* tmpbuf = (char*)kgl_pnalloc(pool, INT2STRING_LEN);
		int len = int2string2(value, tmpbuf);
		return response_header(attr, tmpbuf, len, true);
	}
	bool response_content_length(int64_t content_len) {
		if (content_len >= 0) {
			return response_header(kgl_header_content_length, content_len);
		}
		//无content-length时
		if (data.http_version==0x100) {
			//HTTP/1.0 client not support transfer-encoding
			//The connection MUST close
			KBIT_SET(data.flags, RQ_CONNECTION_CLOSE);
		} else if (!KBIT_TEST(data.flags, RQ_CONNECTION_UPGRADE) && set_transfer_chunked()) {
			KBIT_SET(data.flags, RQ_TE_CHUNKED);
		}
		return true;
	}
	bool response_status(uint16_t status_code)
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
		return response_header(kgl_header_type_string[(int)know_header].value.data, (int)kgl_header_type_string[(int)know_header].value.len, val, val_len);
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
	virtual int64_t get_response_left() {
		return -1;
	}
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
	void set_state(uint8_t state) {		
#ifdef ENABLE_STAT_STUB
		if (data.state == state) {
			return;
		}
		switch (data.state) {
		case STATE_IDLE:
			katom_dec((void*)&kgl_waiting);
			break;
		case STATE_RECV:
			katom_dec((void*)&kgl_reading);
			break;
		case STATE_SEND:
			katom_dec((void*)&kgl_writing);
			break;
		}
#endif
		data.state = state;
#ifdef ENABLE_STAT_STUB
		switch (state) {
		case STATE_IDLE:
			katom_inc((void*)&kgl_waiting);
			break;
		case STATE_RECV:
			katom_inc((void*)&kgl_reading);
			break;
		case STATE_SEND:
			katom_inc((void*)&kgl_writing);
			break;
		}
#endif

	}
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
	virtual void flush() {
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
	virtual void end_request() {

	}
	void set_client_ip(const char* ip)
	{
		if (data.client_ip) {
			xfree(data.client_ip);
		}
		data.client_ip = strdup(ip);
	}

	bool start_response_body(INT64 body_len) {
		assert(!KBIT_TEST(data.flags, RQ_HAS_SEND_HEADER));
		if (KBIT_TEST(data.flags, RQ_HAS_SEND_HEADER)) {
			return true;
		}
		KBIT_SET(data.flags, RQ_HAS_SEND_HEADER);
		if (data.meth == METH_HEAD) {
			body_len = 0;
		}
		int header_len = internal_start_response_body(body_len, false);
		add_down_flow(header_len);
		return header_len >= 0;
	}
	int read(char* buf, int len);
	/* 
	* write_all will send data until all success or failed.
	* if failed return left byte.
	* if return 0 will all success.
	*/
	virtual int write_all(const char* buf, int len) = 0;
	virtual int write_all(const kbuf* buf, int len) {
		while (len > 0) {
			int got = KGL_MIN(len, buf->used);
			len -= got;
			int left = write_all(buf->data, got);
			if (left > 0) {
				return len + left;
			}
			buf = buf->next;
		}
		return len;
	}
	bool parse_header(const char* attr, int attr_len, const char* val, int val_len, bool is_first);
	bool begin_request() {
		katom_inc64((void*)&kgl_total_requests);
		set_state(STATE_RECV);
		assert(data.url == NULL);
		if (data.raw_url == nullptr) {
			return false;
		}
		data.url = new KUrl;
		if (data.raw_url->host) {
			data.url->host = xstrdup(data.raw_url->host);
		}
		if (data.raw_url->param) {
			data.url->param = strdup(data.raw_url->param);
		}
		data.url->flag_encoding = data.raw_url->flag_encoding;
		data.url->port = data.raw_url->port;
		if (data.raw_url->path) {
			data.url->path = xstrdup(data.raw_url->path);
			url_decode(data.url->path, 0, data.url, false);
		}
		return true;
	}
	virtual bool is_locked() = 0;
	kgl_precondition_flag get_precondition_flag() {
		return (kgl_precondition_flag)(data.flags & KGL_FLAG_PRECONDITION_MASK);
	}
	kgl_precondition* get_precondition(kgl_precondition_flag* flag) {
		*flag = get_precondition_flag();
		if (data.precondition.entity) {
			return &data.precondition;
		}
		return NULL;
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
	kgl_len_str_t* alloc_entity(const char* entity_value, int len) {
		kgl_len_str_t* entity = (kgl_len_str_t*)kgl_pnalloc(pool, kgl_len_str_size(len));
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
	void start_parse() {
		data.start_parse();
		if (KBIT_TEST(get_server_model(), WORK_MODEL_SSL)) {
			KBIT_SET(data.raw_url->flags, KGL_URL_SSL | KGL_URL_ORIG_SSL);
		}
	}
	void reset_pipeline() {
		data.clean();
		data.init();
		if (pool) {
			kgl_destroy_pool(pool);
			pool = kgl_create_pool(KGL_REQUEST_POOL_SIZE);
		}
		set_state(STATE_IDLE);
	}
	void init_pool(kgl_pool_t* pool) {
		this->pool = pool;
		if (this->pool == NULL) {
			this->pool = kgl_create_pool(KGL_REQUEST_POOL_SIZE);
		}
	}
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
};
bool kgl_init_sink_queue();
typedef bool (*kgl_sink_iterator)(void* ctx, KSink* sink);
void kgl_iterator_sink(kgl_sink_iterator it, void* ctx);
#endif
