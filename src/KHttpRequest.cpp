#include "KHttpRequest.h"
#include "kselector.h"
#include "KSink.h"
#include "klog.h"
#include "KHttpLib.h"
#include "KHttpFieldValue.h"

KHttpRequest::~KHttpRequest()
{
	delete sink;
	clean();
	FreeLazyMemory();
}
void KHttpRequest::clean()
{
	while (fh) {
		KFlowInfoHelper* fh_next = fh->next;
		delete fh;
		fh = fh_next;
	}
	if (url) {
		url->destroy();
		delete url;
		url = NULL;
	}
	if (pool) {
		kgl_destroy_pool(pool);
		pool = NULL;
	}
}
void KHttpRequest::init(kgl_pool_t* pool)
{
	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));
	InitPool(pool);
	req.begin_time_msec = kgl_current_msec;
}
void KHttpRequest::FreeLazyMemory() {
	if (client_ip) {
		xfree(client_ip);
		client_ip = NULL;
	}
	raw_url.destroy();
	free_header(header);
	header = last = NULL;
}
void KHttpRequest::start_parse() {
	FreeLazyMemory();
	if (KBIT_TEST(sink->GetBindServer()->flags, WORK_MODEL_SSL)) {
		KBIT_SET(raw_url.flags, KGL_URL_SSL | KGL_URL_ORIG_SSL);
	}
	req.meth = METH_UNSET;
}
bool KHttpRequest::parse_header(const char* attr, int attr_len, char* val, int val_len, bool is_first)
{
	if (is_first) {
		start_parse();
	}
	//printf("attr=[%s] val=[%s]\n", attr, val);
	kgl_header_result ret = InternalParseHeader(attr, attr_len, val, &val_len, is_first);
	switch (ret) {
	case kgl_header_failed:
		return false;
	case kgl_header_insert_begin:
		return AddHeader(attr, attr_len, val, val_len, false);
	case kgl_header_success:
		return AddHeader(attr, attr_len, val, val_len, true);
	default:
		return true;
	}
}

bool KHttpRequest::parse_method(const char* src) {
	req.meth = KHttpKeyValue::getMethod(src);
	return req.meth >= 0;
}
bool KHttpRequest::parse_connect_url(char* src) {
	char* ss;
	ss = strchr(src, ':');
	if (!ss) {
		return false;
	}
	KBIT_CLR(raw_url.flags, KGL_URL_ORIG_SSL);
	*ss = 0;
	raw_url.host = strdup(src);
	raw_url.port = atoi(ss + 1);
	return true;
}
kgl_header_result KHttpRequest::parse_host(char* val)
{
	if (raw_url.host == NULL) {
		char* p = NULL;
		if (*val == '[') {
			KBIT_SET(raw_url.flags, KGL_URL_IPV6);
			val++;
			raw_url.host = strdup(val);
			p = strchr(raw_url.host, ']');
			if (p) {
				*p = '\0';
				p = strchr(p + 1, ':');
			}
		}
		else {
			raw_url.host = strdup(val);
			p = strchr(raw_url.host, ':');
			if (p) {
				*p = '\0';
			}
		}
		if (p) {
			raw_url.port = atoi(p + 1);
		}
		else {
			if (KBIT_TEST(raw_url.flags, KGL_URL_SSL)) {
				raw_url.port = 443;
			}
			else {
				raw_url.port = 80;
			}
		}
	}
	return kgl_header_no_insert;
}
bool KHttpRequest::parse_http_version(char* ver) {
	char* dot = strchr(ver, '.');
	if (dot == NULL) {
		return false;
	}
	req.http_major = *(dot - 1) - 0x30;//major;
	req.http_minor = *(dot + 1) - 0x30;//minor;
	return true;
}
kgl_header_result KHttpRequest::InternalParseHeader(const char* attr, int attr_len, char* val, int* val_len, bool is_first)
{
#ifdef ENABLE_HTTP2
	if (this->req.http_major > 1 && *attr == ':') {
		attr++;
		if (strcmp(attr, "method") == 0) {
			if (!parse_method(val)) {
				klog(KLOG_DEBUG, "httpparse:cann't parse meth=[%s]\n", attr);
				return kgl_header_failed;
			}
			return kgl_header_no_insert;
		}
		if (strcmp(attr, "version") == 0) {
			parse_http_version(val);
			return kgl_header_no_insert;
		}
		if (strcmp(attr, "path") == 0) {
			parse_url(val, &raw_url);
			return kgl_header_no_insert;
		}
		if (strcmp(attr, "authority") == 0) {
			if (kgl_header_success == parse_host(val)) {
				//转换成HTTP/1的http头
				AddHeader(kgl_expand_string("Host"), val, *val_len, true);
			}
			return kgl_header_no_insert;
		}
		return kgl_header_no_insert;
	}
#endif
	if (is_first && req.http_major <= 1) {
		if (!parse_method(attr)) {
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
		if (req.meth == METH_CONNECT) {
			result = parse_connect_url(val);
		}
		else {
			result = parse_url(val, &raw_url);
		}
		if (!result) {
			klog(KLOG_DEBUG, "httpparse:cann't parse url [%s]\n", val);
			return kgl_header_failed;
		}
		if (!parse_http_version(space)) {
			klog(KLOG_DEBUG, "httpparse:cann't parse http version [%s]\n", space);
			return kgl_header_failed;
		}
		if (req.http_major > 1 || (req.http_major == 1 && req.http_minor == 1)) {
			KBIT_SET(req.flags, RQ_HAS_KEEP_CONNECTION);
		}
		return kgl_header_no_insert;
	}
	if (!strcasecmp(attr, "Host")) {
		return parse_host(val);
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
				req.flags |= RQ_HAS_KEEP_CONNECTION;
			}
			else if (field.is2("upgrade", 7)) {
				req.flags |= RQ_HAS_CONNECTION_UPGRADE;
			}
			else if (field.is2(kgl_expand_string("close"))) {
				KBIT_CLR(req.flags, RQ_HAS_KEEP_CONNECTION);
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
				KBIT_SET(raw_url.accept_encoding, KGL_ENCODING_GZIP);
			}
			else if (field.is2(kgl_expand_string("deflate"))) {
				KBIT_SET(raw_url.accept_encoding, KGL_ENCODING_DEFLATE);
			}
			else if (field.is2(kgl_expand_string("compress"))) {
				KBIT_SET(raw_url.accept_encoding, KGL_ENCODING_COMPRESS);
			}
			else if (field.is2(kgl_expand_string("br"))) {
				KBIT_SET(raw_url.accept_encoding, KGL_ENCODING_BR);
			}
			else if (!field.is2(kgl_expand_string("identity"))) {
				KBIT_SET(raw_url.accept_encoding, KGL_ENCODING_UNKNOW);
			}
		} while (field.next());
		return kgl_header_success;
	}
	if (!strcasecmp(attr, "If-Range")) {
		time_t try_time = parse1123time(val);
		if (try_time == -1) {
			req.flags |= RQ_IF_RANGE_ETAG;
			if (if_none_match == NULL) {
				set_if_none_match(val, *val_len);
			}
		} else {
			req.if_modified_since = try_time;
			req.flags |= RQ_IF_RANGE_DATE;
		}
		return kgl_header_no_insert;
	}
	if (!strcasecmp(attr, "If-Modified-Since")) {
		req.if_modified_since = parse1123time(val);
		req.flags |= RQ_HAS_IF_MOD_SINCE;
		return kgl_header_no_insert;
	}
	if (!strcasecmp(attr, "If-None-Match")) {
		req.flags |= RQ_HAS_IF_NONE_MATCH;
		if (if_none_match == NULL) {
			set_if_none_match(val, *val_len);
		}
		return kgl_header_no_insert;
	}
	//	printf("attr=[%s],val=[%s]\n",attr,val);
	if (!strcasecmp(attr, "Content-length")) {
		req.content_length = string2int(val);
		req.left_read = req.content_length;
		req.flags |= RQ_HAS_CONTENT_LEN;
		return kgl_header_no_insert;
	}
	if (strcasecmp(attr, "Transfer-Encoding") == 0) {
		if (strcasecmp(val, "chunked") == 0) {
			KBIT_SET(req.flags, RQ_INPUT_CHUNKED);
			req.content_length = -1;
		}
		return kgl_header_no_insert;
	}
	if (!strcasecmp(attr, "Expect")) {
		if (strstr(val, "100-continue") != NULL) {
			req.flags |= RQ_HAVE_EXPECT;
		}
		return kgl_header_no_insert;
	}
	if (!strcasecmp(attr, "X-Forwarded-Proto")) {
		if (strcasecmp(val, "https") == 0) {
			KBIT_SET(raw_url.flags, KGL_URL_ORIG_SSL);
		}
		else {
			KBIT_CLR(raw_url.flags, KGL_URL_ORIG_SSL);
		}
		return kgl_header_no_insert;
	}
	if (!strcasecmp(attr, "Pragma")) {
		if (strstr(val, "no-cache"))
			req.flags |= RQ_HAS_NO_CACHE;
		return kgl_header_success;
	}
#if 0
	if (
		//{{ent
#ifdef HTTP_PROXY
		(KBIT_TEST(GetWorkModel(), WORK_MODEL_MANAGE) && !strcasecmp(attr, "Authorization")) ||
#endif//}}
		!strcasecmp(attr, AUTH_REQUEST_HEADER)) {
		//{{ent
#ifdef HTTP_PROXY
		flags |= RQ_HAS_PROXY_AUTHORIZATION;
#else//}}
		req.flags |= RQ_HAS_AUTHORIZATION;
		//{{ent
#endif//}}
#ifdef ENABLE_TPROXY
		if (!KBIT_TEST(GetWorkModel(), WORK_MODEL_TPROXY)) {
#endif
			char* p = val;
			while (*p && !IS_SPACE(*p)) {
				p++;
			}
			char* p2 = p;
			while (*p2 && IS_SPACE(*p2)) {
				p2++;
			}
			KHttpAuth* tauth = NULL;
			if (strncasecmp(val, "basic", p - val) == 0) {
				tauth = new KHttpBasicAuth;
			}
			else if (strncasecmp(val, "digest", p - val) == 0) {
#ifdef ENABLE_DIGEST_AUTH
				tauth = new KHttpDigestAuth;
#endif
			}
			if (tauth) {
				if (!tauth->parse(this, p2)) {
					delete tauth;
					tauth = NULL;
				}
			}
			if (auth) {
				delete auth;
			}
			auth = tauth;
			//{{ent
#ifdef HTTP_PROXY
			return kgl_header_no_insert;
#endif//}}
#ifdef ENABLE_TPROXY
		}
#endif
		return kgl_header_success;
	}
#endif
	if (!strcasecmp(attr, "Cache-Control")) {
		KHttpFieldValue field(val);
		do {
			if (field.is("no-store") || field.is("no-cache")) {
				req.flags |= RQ_HAS_NO_CACHE;
			}
			else if (field.is("only-if-cached")) {
				req.flags |= RQ_HAS_ONLY_IF_CACHED;
			}
		} while (field.next());
		return kgl_header_success;
	}
	if (!strcasecmp(attr, "Range")) {
		if (!strncasecmp(val, "bytes=", 6)) {
			val += 6;
			req.range_from = -1;
			req.range_to = -1;
			if (*val != '-') {
				req.range_from = string2int(val);
			}
			char* p = strchr(val, '-');
			if (p && *(p + 1)) {
				req.range_to = string2int(p + 1);
			}
			char* next_range = strchr(val, ',');
			if (next_range) {
				//we do not support multi range
				klog(KLOG_INFO, "cut multi_range %s\n", val);
				//KBIT_SET(filter_flags,RF_NO_CACHE);
				*next_range = '\0';
			}
		}
		req.flags |= RQ_HAVE_RANGE;
		return kgl_header_success;
	}
#if 0
	if (!strcasecmp(attr, "Content-Type")) {
#ifdef ENABLE_INPUT_FILTER
		if (if_ctx == NULL) {
			if_ctx = new KInputFilterContext(this);
		}
#endif
		if (strncasecmp(val, "multipart/form-data", 19) == 0) {
			KBIT_SET(req.flags, RQ_POST_UPLOAD);
#ifdef ENABLE_INPUT_FILTER
			if_ctx->parseBoundary(val + 19);
#endif
		}
		return kgl_header_success;
	}
#endif
	return kgl_header_success;
}

int KHttpRequest::write(WSABUF* buf, int bc)
{
	int got = sink->Write(buf, bc);
	if (got > 0) {
		add_down_flow(got);
	}
	return got;
}
int KHttpRequest::write(const char* buf, int len)
{
	WSABUF bufs;
	bufs.iov_base = (char*)buf;
	bufs.iov_len = len;
	return write(&bufs, 1);
}
bool KHttpRequest::write(kbuf* buf)
{
	while (buf) {
		if (!write_full(buf->data, buf->used)) {
			return false;
		}
		buf = buf->next;
	}
	return true;
}
bool KHttpRequest::write_full(const char* buf, int len)
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
bool KHttpRequest::response_content_length(int64_t content_len)
{
	if (content_len >= 0) {
		//有content-length时
		char len_str[INT2STRING_LEN];
		int len = int2string2(content_len, len_str, false);
		return response_header(kgl_expand_string("Content-Length"), len_str, len);
	}
	//无content-length时
	if (req.http_minor == 0) {
		//A HTTP/1.0 client no support TE head.
		//The connection MUST close
		KBIT_SET(req.flags, RQ_CONNECTION_CLOSE);
	}
	else if (!res.connection_upgrade && sink->SetTransferChunked()) {
		res.te_chunked = 1;
	}
	return true;
}
int KHttpRequest::end_request()
{
	return sink->EndRequest(this);
}
bool KHttpRequest::start_response_body(INT64 body_len)
{
	assert(!res.header_has_send);
	if (res.header_has_send) {
		return true;
	}
	res.header_has_send = 1;
	if (req.meth == METH_HEAD) {
		body_len = 0;
	}
	int header_len = sink->StartResponseBody(this, body_len);
	add_down_flow(header_len, true);
	return header_len > 0;
}
