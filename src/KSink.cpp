#include "KSink.h"
#include "klog.h"
#include "KHttpFieldValue.h"
#include "kfiber.h"

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
	int header_len = StartResponseBody(body_len);
	add_down_flow(header_len, true);
	return header_len > 0;
}
void KSink::begin_request()
{
#ifdef ENABLE_STAT_STUB
	katom_inc64((void*)&kgl_total_requests);
#endif
	//setState(STATE_RECV);
	assert(data.url == NULL);
	data.url = new KUrl;
	if (data.raw_url.host) {
		data.url->host = xstrdup(data.raw_url.host);
	}
	if (data.raw_url.param) {
		data.url->param = strdup(data.raw_url.param);
	}
	data.url->flag_encoding = data.raw_url.flag_encoding;
	data.url->port = data.raw_url.port;
	if (data.raw_url.path) {
		data.url->path = xstrdup(data.raw_url.path);
		url_decode(data.url->path, 0, data.url, false);
	}
	if (KBIT_TEST(data.flags, RQ_INPUT_CHUNKED)) {
		data.left_read = -1;
	} else {
		data.left_read = data.content_length;
	}
}
bool KSink::parse_header(const char* attr, int attr_len, char* val, int val_len, bool is_first)
{
	if (is_first) {
		start_parse();
	}
	kgl_header_result ret = internal_parse_header(attr, attr_len, val, &val_len, is_first);
	switch (ret) {
	case kgl_header_failed:
		return false;
	case kgl_header_insert_begin:
		return data.AddHeader(attr, attr_len, val, val_len, false);
	case kgl_header_success:
		return data.AddHeader(attr, attr_len, val, val_len, true);
	default:
		return true;
	}
}

kgl_header_result KSink::internal_parse_header(const char* attr, int attr_len, char* val, int* val_len, bool is_first)
{
#ifdef ENABLE_HTTP2
	if (data.http_major > 1 && *attr == ':') {
		attr++;
		if (strcmp(attr, "method") == 0) {
			if (!data.parse_method(val)) {
				klog(KLOG_DEBUG, "httpparse:cann't parse meth=[%s]\n", attr);
				return kgl_header_failed;
			}
			return kgl_header_no_insert;
		}
		if (strcmp(attr, "version") == 0) {
			data.parse_http_version(val);
			return kgl_header_no_insert;
		}
		if (strcmp(attr, "path") == 0) {
			parse_url(val, &data.raw_url);
			return kgl_header_no_insert;
		}
		if (strcmp(attr, "authority") == 0) {
			if (kgl_header_success == data.parse_host(val)) {
				//转换成HTTP/1的http头
				data.AddHeader(kgl_expand_string("Host"), val, *val_len, true);
			}
			return kgl_header_no_insert;
		}
		return kgl_header_no_insert;
	}
#endif
	if (is_first && data.http_major <= 1) {
		if (!data.parse_method(attr)) {
			klog(KLOG_DEBUG, "httpparse:cann't parse meth=[%s]\n", attr);
			return kgl_header_failed;
		}
		char* space = strchr(val, ' ');
		if (space == NULL) {
			klog(KLOG_DEBUG, "httpparse:cann't get space seperator to parse HTTP/1.1 [%s]\n", val);
			return kgl_header_failed;
		}
		*space = 0;
		space++;

		while (*space && IS_SPACE(*space)) {
			space++;
		}
		bool result;
		if (data.meth == METH_CONNECT) {
			result = data.parse_connect_url(val);
		} else {
			result = parse_url(val, &data.raw_url);
		}
		if (!result) {
			klog(KLOG_DEBUG, "httpparse:cann't parse url [%s]\n", val);
			return kgl_header_failed;
		}
		if (!data.parse_http_version(space)) {
			klog(KLOG_DEBUG, "httpparse:cann't parse http version [%s]\n", space);
			return kgl_header_failed;
		}
		if (data.http_major > 1 || (data.http_major == 1 && data.http_minor == 1)) {
			KBIT_SET(data.flags, RQ_HAS_KEEP_CONNECTION);
		}
		return kgl_header_no_insert;
	}
	if (!strcasecmp(attr, "Host")) {
		return data.parse_host(val);
	}
	if (!strcasecmp(attr, "Connection")
		//{{ent
#ifdef HTTP_PROXY
		|| !strcasecmp(attr, "proxy-connection")
#endif//}}
		) {
		KHttpFieldValue field(val);
		do {
			if (field.is2("keep-alive", 10)) {
				data.flags |= RQ_HAS_KEEP_CONNECTION;
			} else if (field.is2("upgrade", 7)) {
				data.flags |= RQ_HAS_CONNECTION_UPGRADE;
			} else if (field.is2(kgl_expand_string("close"))) {
				KBIT_CLR(data.flags, RQ_HAS_KEEP_CONNECTION);
			}
		} while (field.next());
		return kgl_header_success;
	}
	if (!strcasecmp(attr, "Accept-Encoding")) {
		if (!*val) {
			return kgl_header_no_insert;
		}
		KHttpFieldValue field(val);
		do {
			if (field.is2(kgl_expand_string("gzip"))) {
				KBIT_SET(data.raw_url.accept_encoding, KGL_ENCODING_GZIP);
			} else if (field.is2(kgl_expand_string("deflate"))) {
				KBIT_SET(data.raw_url.accept_encoding, KGL_ENCODING_DEFLATE);
			} else if (field.is2(kgl_expand_string("compress"))) {
				KBIT_SET(data.raw_url.accept_encoding, KGL_ENCODING_COMPRESS);
			} else if (field.is2(kgl_expand_string("br"))) {
				KBIT_SET(data.raw_url.accept_encoding, KGL_ENCODING_BR);
			} else if (!field.is2(kgl_expand_string("identity"))) {
				KBIT_SET(data.raw_url.accept_encoding, KGL_ENCODING_UNKNOW);
			}
		} while (field.next());
		return kgl_header_success;
	}
	if (!strcasecmp(attr, "If-Range")) {
		time_t try_time = parse1123time(val);
		if (try_time == -1) {
			data.flags |= RQ_IF_RANGE_ETAG;
			if (data.if_none_match == NULL) {
				set_if_none_match(val, *val_len);
			}
		} else {
			data.if_modified_since = try_time;
			data.flags |= RQ_IF_RANGE_DATE;
		}
		return kgl_header_no_insert;
	}
	if (!strcasecmp(attr, "If-Modified-Since")) {
		data.if_modified_since = parse1123time(val);
		data.flags |= RQ_HAS_IF_MOD_SINCE;
		return kgl_header_no_insert;
	}
	if (!strcasecmp(attr, "If-None-Match")) {
		data.flags |= RQ_HAS_IF_NONE_MATCH;
		if (data.if_none_match == NULL) {
			set_if_none_match(val, *val_len);
		}
		return kgl_header_no_insert;
	}
	//	printf("attr=[%s],val=[%s]\n",attr,val);
	if (!strcasecmp(attr, "Content-length")) {
		data.content_length = string2int(val);
		data.left_read = data.content_length;
		data.flags |= RQ_HAS_CONTENT_LEN;
		return kgl_header_no_insert;
	}
	if (strcasecmp(attr, "Transfer-Encoding") == 0) {
		if (strcasecmp(val, "chunked") == 0) {
			KBIT_SET(data.flags, RQ_INPUT_CHUNKED);
			data.content_length = -1;
		}
		return kgl_header_no_insert;
	}
	if (!strcasecmp(attr, "Expect")) {
		if (strstr(val, "100-continue") != NULL) {
			data.flags |= RQ_HAVE_EXPECT;
		}
		return kgl_header_no_insert;
	}
	if (!strcasecmp(attr, "X-Forwarded-Proto")) {
		if (strcasecmp(val, "https") == 0) {
			KBIT_SET(data.raw_url.flags, KGL_URL_ORIG_SSL);
		} else {
			KBIT_CLR(data.raw_url.flags, KGL_URL_ORIG_SSL);
		}
		return kgl_header_no_insert;
	}
	if (!strcasecmp(attr, "Pragma")) {
		if (strstr(val, "no-cache"))
			data.flags |= RQ_HAS_NO_CACHE;
		return kgl_header_success;
	}
	if (!strcasecmp(attr, "Cache-Control")) {
		KHttpFieldValue field(val);
		do {
			if (field.is("no-store") || field.is("no-cache")) {
				data.flags |= RQ_HAS_NO_CACHE;
			} else if (field.is("only-if-cached")) {
				data.flags |= RQ_HAS_ONLY_IF_CACHED;
			}
		} while (field.next());
		return kgl_header_success;
	}
	if (!strcasecmp(attr, "Range")) {
		if (!strncasecmp(val, "bytes=", 6)) {
			val += 6;
			data.range_from = -1;
			data.range_to = -1;
			if (*val != '-') {
				data.range_from = string2int(val);
			}
			char* p = strchr(val, '-');
			if (p && *(p + 1)) {
				data.range_to = string2int(p + 1);
			}
			char* next_range = strchr(val, ',');
			if (next_range) {
				//we do not support multi range
				klog(KLOG_INFO, "cut multi_range %s\n", val);
				//KBIT_SET(filter_flags,RF_NO_CACHE);
				*next_range = '\0';
			}
		}
		data.flags |= RQ_HAVE_RANGE;
		return kgl_header_success;
	}
	return kgl_header_success;
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
}
void KSink::start_parse() {
	data.start_parse();
	if (KBIT_TEST(GetBindServer()->flags, WORK_MODEL_SSL)) {
		KBIT_SET(data.raw_url.flags, KGL_URL_SSL | KGL_URL_ORIG_SSL);
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
	} else if (!KBIT_TEST(data.flags, RQ_CONNECTION_UPGRADE) && SetTransferChunked()) {
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
		Flush();
		start_header();
	}
	int length;
	if (data.left_read >= 0 && !KBIT_TEST(data.flags, RQ_CONNECTION_UPGRADE)) {
		len = (int)MIN((int64_t)len, data.left_read);
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
		if (!write_full(buf->data, buf->used)) {
			return false;
		}
		buf = buf->next;
	}
	return true;
}
bool KSink::write_full(const char* buf, int len)
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