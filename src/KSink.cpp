#include "KSink.h"
#include "klog.h"
#include "KHttpFieldValue.h"
#include "kfiber.h"
#include "kselector_manager.h"
#include "kfiber_sync.h"

struct kgl_request_ts
{
	kgl_list sinks;
};
static pthread_key_t kgl_request_key;
kev_result kgl_request_thread_init(KOPAQUE data, void* arg, int got)
{
	kgl_request_ts* rq = new kgl_request_ts;
	klist_init(&rq->sinks);
	pthread_setspecific(kgl_request_key, rq);
	return kev_ok;
}
struct kgl_sink_iterator_param
{
	void* ctx;
	kgl_sink_iterator it;
	kfiber_cond* cond;
};
static kev_result ksink_iterator(KOPAQUE data, void* arg, int got)
{
	kgl_sink_iterator_param* param = (kgl_sink_iterator_param*)arg;
	kgl_request_ts* ts = (kgl_request_ts*)pthread_getspecific(kgl_request_key);
	kgl_list* pos;
	klist_foreach(pos, &ts->sinks) {
		KSink* sink = (KSink*)kgl_list_data(pos, KSink, queue);
		if (!param->it(param->ctx, sink)) {
			break;
		}
	}
	param->cond->f->notice(param->cond, got);
	return kev_ok;
}
void kgl_iterator_sink(kgl_sink_iterator it, void* ctx)
{
	kgl_sink_iterator_param param;
	param.ctx = ctx;
	param.it = it;
	param.cond = kfiber_cond_init_ts(true);
	auto selector_count = get_selector_count();
	for (int i = 0; i < selector_count; i++) {
		kselector* selector = get_selector_by_index(i);
		kgl_selector_module.next(selector, NULL, ksink_iterator, &param, 0);
		param.cond->f->wait(param.cond);
	}
	param.cond->f->release(param.cond);
}
KSink::KSink(kgl_pool_t* pool)
{
	init_pool(pool);
	kgl_request_ts* ts = (kgl_request_ts*)pthread_getspecific(kgl_request_key);
	assert(ts);
	klist_append(&ts->sinks, &queue);
}
KSink::~KSink()
{
	if (pool) {
		kgl_destroy_pool(pool);
	}
	set_state(STATE_UNKNOW);
	klist_remove(&queue);
}
bool KSink::start_response_body(INT64 body_len)
{
	assert(!KBIT_TEST(data.flags, RQ_HAS_SEND_HEADER));
	if (KBIT_TEST(data.flags, RQ_HAS_SEND_HEADER)) {
		return true;
	}
	KBIT_SET(data.flags, RQ_HAS_SEND_HEADER);
	if (data.meth == METH_HEAD) {
		body_len = 0;
	}
	int header_len = internal_start_response_body(body_len);
	add_down_flow(header_len, true);
	return header_len > 0;
}
bool KSink::begin_request()
{
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
	if (KBIT_TEST(data.flags, RQ_INPUT_CHUNKED)) {
		data.left_read = -1;
	} else {
		data.left_read = data.content_length;
	}
	return true;
}
bool KSink::parse_header(const char* attr, int attr_len, char* val, int val_len, bool is_first)
{
	//printf("attr=[%s],val=[%s]\n",attr,val);
	if (is_first) {
		start_parse();
	}
#if defined(ENABLE_HTTP2) || defined(ENABLE_HTTP3)
	if (data.http_major > 1 && *attr == ':') {
		if (kgl_mem_same(attr, attr_len, kgl_expand_string(":method"))) {
			return data.parse_method(val, val_len);
		}
		if (kgl_mem_same(attr, attr_len, kgl_expand_string(":version"))) {
			return data.parse_http_version((u_char*)val, val_len);
		}
		if (kgl_mem_same(attr, attr_len, kgl_expand_string(":path"))) {
			return parse_url(val, val_len, data.raw_url);
		}
		if (kgl_mem_same(attr, attr_len, kgl_expand_string(":authority"))) {
			return data.parse_host(val, val_len);
		}
		return true;
	}
#endif
	if (is_first && data.http_major <= 1) {
		if (!data.parse_method(attr, attr_len)) {
			//klog(KLOG_DEBUG, "httpparse:cann't parse meth=[%s]\n", attr);
			return false;
		}
		u_char* space = (u_char*)memchr(val, ' ', val_len);
		if (space == NULL) {
			//klog(KLOG_DEBUG, "httpparse:cann't get space seperator to parse HTTP/1.1 [%s]\n", val);
			return false;
		}
		*space = 0;
		size_t url_len = space - (u_char*)val;
		while (*space && IS_SPACE(*space)) {
			space++;
		}
		bool result;
		if (data.meth == METH_CONNECT) {
			result = data.parse_connect_url((u_char*)val, url_len);
		} else {
			result = parse_url(val, url_len, data.raw_url);
		}
		if (!result) {
			//klog(KLOG_DEBUG, "httpparse:cann't parse url [%s]\n", val);
			return false;
		}
		if (!data.parse_http_version(space, val_len - url_len)) {
			//klog(KLOG_DEBUG, "httpparse:cann't parse http version [%s]\n", space);
			return false;
		}
		if (data.http_major > 1 || (data.http_major == 1 && data.http_minor == 1)) {
			KBIT_SET(data.flags, RQ_HAS_KEEP_CONNECTION);
		}
		return true;
	}
	if (kgl_mem_case_same(attr, attr_len, kgl_expand_string("Host"))) {
		return data.parse_host(val, val_len);
	}
	if (kgl_mem_case_same(attr, attr_len, kgl_expand_string("Connection"))
#ifdef HTTP_PROXY
		|| mem_case_same(attr, attr_len, kgl_expand_string("proxy-connection"))
#endif
		) {
		KHttpFieldValue field(val, val + val_len);
		do {
			if (field.is(_KS("keep-alive"))) {
				data.flags |= RQ_HAS_KEEP_CONNECTION;
			} else if (field.is(_KS("upgrade"))) {
				data.flags |= RQ_HAS_CONNECTION_UPGRADE;
			} else if (field.is(_KS("close"))) {
				KBIT_CLR(data.flags, RQ_HAS_KEEP_CONNECTION);
			}
		} while (field.next());
		return data.add_header(kgl_header_connection, val, val_len);
	}
	if (kgl_mem_case_same(attr, attr_len, kgl_expand_string("Accept-Encoding"))) {
		KHttpFieldValue field(val, val + val_len);
		do {
			if (field.is(kgl_expand_string("gzip"))) {
				KBIT_SET(data.raw_url->accept_encoding, KGL_ENCODING_GZIP);
			} else if (field.is(kgl_expand_string("deflate"))) {
				KBIT_SET(data.raw_url->accept_encoding, KGL_ENCODING_DEFLATE);
			} else if (field.is(kgl_expand_string("compress"))) {
				KBIT_SET(data.raw_url->accept_encoding, KGL_ENCODING_COMPRESS);
			} else if (field.is(kgl_expand_string("br"))) {
				KBIT_SET(data.raw_url->accept_encoding, KGL_ENCODING_BR);
			} else if (!field.is(kgl_expand_string("identity"))) {
				KBIT_SET(data.raw_url->accept_encoding, KGL_ENCODING_UNKNOW);
			}
		} while (field.next());
		return data.add_header(kgl_header_accept_encoding, val, val_len);
	}
	if (kgl_mem_case_same(attr, attr_len, kgl_expand_string("If-Range"))) {
		time_t try_time = kgl_parse_http_time((u_char*)val, val_len);
		if (try_time == -1) {
			data.flags |= RQ_IF_RANGE_ETAG;
			if (data.if_none_match == NULL) {
				set_if_none_match(val, val_len);
			}
		} else {
			data.if_modified_since = try_time;
			data.flags |= RQ_IF_RANGE_DATE;
		}
		return true;
	}
	if (kgl_mem_case_same(attr, attr_len, kgl_expand_string("If-Modified-Since"))) {
		data.if_modified_since = kgl_parse_http_time((u_char*)val, val_len);
		data.flags |= RQ_HAS_IF_MOD_SINCE;
		return true;
	}
	if (kgl_mem_case_same(attr, attr_len, kgl_expand_string("If-None-Match"))) {
		data.flags |= RQ_HAS_IF_NONE_MATCH;
		if (data.if_none_match == NULL) {
			set_if_none_match(val, val_len);
		}
		return true;
	}

	if (kgl_mem_case_same(attr, attr_len, kgl_expand_string("Content-length"))) {
		data.content_length = string2int(val);
		data.left_read = data.content_length;
		data.flags |= RQ_HAS_CONTENT_LEN;
		return true;
	}
	if (kgl_mem_case_same(attr, attr_len, kgl_expand_string("Transfer-Encoding"))) {
		if (kgl_mem_case_same(val, val_len, kgl_expand_string("chunked"))) {
			KBIT_SET(data.flags, RQ_INPUT_CHUNKED);
			data.content_length = -1;
		}
		return true;
	}
	if (kgl_mem_case_same(attr, attr_len, kgl_expand_string("Expect"))) {
		if (kgl_memstr(val, val_len, kgl_expand_string("100-continue")) != NULL) {
			data.flags |= RQ_HAVE_EXPECT;
		}
		return true;
	}
	if (kgl_mem_case_same(attr, attr_len, kgl_expand_string("X-Forwarded-Proto"))) {
		if (kgl_mem_case_same(val, val_len, kgl_expand_string("https"))) {
			KBIT_SET(data.raw_url->flags, KGL_URL_ORIG_SSL);
		} else {
			KBIT_CLR(data.raw_url->flags, KGL_URL_ORIG_SSL);
		}
		return true;
	}
	if (kgl_mem_case_same(attr, attr_len, kgl_expand_string("Pragma"))) {
		if (kgl_memstr(val, val_len, kgl_expand_string("no-cache"))) {
			data.flags |= RQ_HAS_NO_CACHE;
		}
		return data.add_header(kgl_header_pragma, val, val_len);
	}
	if (kgl_mem_case_same(attr, attr_len, kgl_expand_string("Cache-Control"))) {
		KHttpFieldValue field(val, val + val_len);
		do {
			if (field.is(_KS("no-store")) || field.is(_KS("no-cache"))) {
				data.flags |= RQ_HAS_NO_CACHE;
			} else if (field.is(_KS("only-if-cached"))) {
				data.flags |= RQ_HAS_ONLY_IF_CACHED;
			}
		} while (field.next());
		return data.add_header(kgl_header_cache_control, val, val_len);
	}
	if (kgl_mem_case_same(attr, attr_len, kgl_expand_string("Range"))) {
		data.flags |= RQ_HAVE_RANGE;
		if (val_len > 6 && !strncasecmp(val, kgl_expand_string("bytes="))) {
			u_char* end = (u_char*)val + val_len;
			u_char* hot = (u_char*)val + 6;
			data.range_from = -1;
			data.range_to = -1;
			if (*hot != '-') {
				data.range_from = kgl_atol(hot, end - hot);
			}
			hot = (u_char*)memchr(hot, '-', end - hot);
			if (hot && hot < end - 1) {
				hot++;
				data.range_to = kgl_atol(hot, end - hot);
			}
			u_char* next_range = (u_char*)memchr(hot, ',', end - hot);
			if (next_range) {
				//we do not support multi range
				end = next_range;
				return data.add_header(kgl_header_range, val, (int)(end - (u_char*)val), true);
			}
		}
		return data.add_header(kgl_header_range, val, val_len);
	}
	return data.add_header(attr, attr_len, val, val_len);
}
void KSink::init_pool(kgl_pool_t* pool) {
	this->pool = pool;
	if (this->pool == NULL) {
		this->pool = kgl_create_pool(KGL_REQUEST_POOL_SIZE);
	}
}
void KSink::reset_pipeline()
{
	data.clean();
	data.init();
	if (pool) {
		kgl_destroy_pool(pool);
		pool = kgl_create_pool(KGL_REQUEST_POOL_SIZE);
	}
	set_state(STATE_IDLE);
}
const char* KSink::get_state()
{
	switch (data.state) {
	case STATE_IDLE:
		return "idle";
	case STATE_SEND:
		return "send";
	case STATE_RECV:
		return "recv";
	case STATE_QUEUE:
		return "queue";
	}
	return "unknow";
}
void KSink::set_state(uint8_t state)
{
#ifdef ENABLE_STAT_STUB
	if (data.state == state) {
		return;
	}
	switch (data.state) {
	case STATE_IDLE:
	case STATE_QUEUE:
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
	case STATE_QUEUE:
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
bool KSink::adjust_range(int64_t* len)
{
	if (data.range_from >= 0) {
		if (data.range_from >= (*len)) {
			//klog(KLOG_ERR, "[%s] request [%s%s] range error,request range_from=" INT64_FORMAT ",range_to=" INT64_FORMAT ",len=" INT64_FORMAT "\n", rq->getClientIp(), rq->sink->data.raw_url->host, rq->sink->data.raw_url->path, rq->sink->data.range_from, rq->sink->data.range_to, len);
			return false;
		}
		(*len) -= data.range_from;
		if (data.range_to >= 0) {
			(*len) = KGL_MIN(data.range_to - data.range_from + 1, (*len));
			if ((*len) <= 0) {
				//klog(KLOG_ERR, "[%s] request [%s%s] range error,request range_from=" INT64_FORMAT ",range_to=" INT64_FORMAT ",len=" INT64_FORMAT "\n", rq->getClientIp(), rq->sink->data.raw_url->host, rq->sink->data.raw_url->path, rq->sink->data.range_from, rq->sink->data.range_to, len);
				return false;
			}
		}
	} else if (data.range_from < 0) {
		data.range_from += *len;
		if (data.range_from < 0) {
			data.range_from = 0;
		}
		(*len) -= data.range_from;
	}
	data.range_to = data.range_from + (*len) - 1;
	//printf("after from=%lld,to=%lld,len=%lld\n",rq->sink->data.range_from,rq->sink->data.range_to,len);
	return true;
}
void KSink::start_parse() {
	data.start_parse();
	if (KBIT_TEST(get_server_model(), WORK_MODEL_SSL)) {
		KBIT_SET(data.raw_url->flags, KGL_URL_SSL | KGL_URL_ORIG_SSL);
	}
}
bool KSink::response_content_length(int64_t content_len)
{
	if (content_len >= 0) {
		//有content-length时
		char len_str[INT2STRING_LEN];
		int len = int2string2(content_len, len_str, false);
		return response_header(kgl_expand_string("Content-Length"), len_str, len);
	}
	//无content-length时
	if (data.http_minor == 0) {
		//A HTTP/1.0 client no support TE head.
		//The connection MUST close
		KBIT_SET(data.flags, RQ_CONNECTION_CLOSE);
	} else if (!KBIT_TEST(data.flags, RQ_CONNECTION_UPGRADE) && set_transfer_chunked()) {
		KBIT_SET(data.flags, RQ_TE_CHUNKED);
	}
	return true;
}
int KSink::write(WSABUF* buf, int bc)
{
	int got = internal_write(buf, bc);
	if (got > 0) {
		add_down_flow(got);
	}
	return got;
}
int KSink::write(const char* buf, int len)
{
	WSABUF bufs;
	bufs.iov_base = (char*)buf;
	bufs.iov_len = len;
	return write(&bufs, 1);
}
int KSink::read(char* buf, int len)
{
	kassert(!kfiber_is_main());
	if (KBIT_TEST(data.flags, RQ_HAVE_EXPECT)) {
		KBIT_CLR(data.flags, RQ_HAVE_EXPECT);
		response_status(100);
		start_response_body(0);
		flush();
		start_header();
	}
	int length;
	if (data.left_read >= 0 && !KBIT_TEST(data.flags, RQ_CONNECTION_UPGRADE)) {
		len = (int)KGL_MIN((int64_t)len, data.left_read);
	}
	length = internal_read(buf, len);
	if (length == 0 && data.left_read == -1 && !KBIT_TEST(data.flags, RQ_CONNECTION_UPGRADE)) {
		data.left_read = 0;
		return 0;
	}
	if (length > 0) {
		if (!KBIT_TEST(data.flags, RQ_CONNECTION_UPGRADE) && data.left_read != -1) {
			data.left_read -= length;
		}
		add_up_flow(length);
	}
	return length;
}
bool KSink::write(kbuf* buf)
{
	while (buf) {
		if (!write_all(buf->data, buf->used)) {
			return false;
		}
		buf = buf->next;
	}
	return true;
}
bool KSink::write_all(const char* buf, int len)
{
	while (len > 0) {
		int this_len = write(buf, len);
		if (this_len <= 0) {
			return false;
		}
		len -= this_len;
		buf += this_len;
	}
	return true;
}
bool kgl_init_sink_queue()
{
	pthread_key_create(&kgl_request_key, NULL);
	if (0 != selector_manager_thread_init(kgl_request_thread_init, NULL)) {
		klog(KLOG_ERR, "init_http_server_callback must called early!!!\n");
		return false;
	}
	return true;
}