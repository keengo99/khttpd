#include "KSink.h"
#include "klog.h"
#include "KHttpFieldValue.h"
#include "kfiber.h"
#include "kselector_manager.h"
#include "kfiber_sync.h"
#ifdef KGL_DEBUG_TIME
struct kgl_timespec {
	int64_t total;
	int count;
};
struct kgl_timespec total_spec = { 0 };
#define NS_PER_SECOND 1000000000
void sub_timespec(struct timespec t1, struct timespec t2, struct timespec* td) {
	td->tv_nsec = t2.tv_nsec - t1.tv_nsec;
	td->tv_sec = t2.tv_sec - t1.tv_sec;
	if (td->tv_sec > 0 && td->tv_nsec < 0) {
		td->tv_nsec += NS_PER_SECOND;
		td->tv_sec--;
	} else if (td->tv_sec < 0 && td->tv_nsec>0) {
		td->tv_nsec -= NS_PER_SECOND;
		td->tv_sec++;
	}
}
void KSink::log_passed_time(const char* tip) {
	struct timespec end, delta;
	clock_gettime(CLOCK_REALTIME, &end);
	sub_timespec(start_time, end, &delta);
	klog(KLOG_ERR, "%p %s passed_time=[%d.%09ld]\n", this, tip, (int)delta.tv_sec, delta.tv_nsec);
	if (*tip == '+') {
		int64_t v = delta.tv_sec * NS_PER_SECOND + delta.tv_nsec;
		total_spec.total += v;
		total_spec.count++;
	}
	reset_start_time();
	return;
}
void kgl_log_total_timespec() {
	kgl_timespec a = total_spec;
	if (a.count > 0) {
		klog(KLOG_ERR, "count=[%d] avg time=[%lld]\n", a.count, a.total / a.count);
	}
}
#endif

pthread_key_t kgl_request_key;
kev_result kgl_request_thread_init(KOPAQUE data, void* arg, int got) {
	if (got == 0) {
		//init
		kgl_request_ts* rq = new kgl_request_ts;
		klist_init(&rq->sinks);
		pthread_setspecific(kgl_request_key, rq);
	} else {
		//shutdown
		kgl_request_ts* rq = (kgl_request_ts*)pthread_getspecific(kgl_request_key);
		klist_empty(&rq->sinks);
		delete rq;
		pthread_setspecific(kgl_request_key, NULL);
	}
	return kev_ok;
}
struct kgl_sink_iterator_param
{
	void* ctx;
	kgl_sink_iterator it;
	kfiber_cond* cond;
};
static kev_result ksink_iterator(KOPAQUE data, void* arg, int got) {
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
void kgl_iterator_sink(kgl_sink_iterator it, void* ctx) {
	kgl_sink_iterator_param param;
	param.ctx = ctx;
	param.it = it;
	param.cond = kfiber_cond_init_ts(true);
	auto selector_count = get_selector_count();
	for (int i = 0; i < selector_count; i++) {
		kselector* selector = get_selector_by_index(i);
		kgl_selector_module.next(selector, NULL, ksink_iterator, &param, 0);
		param.cond->f->wait(param.cond, NULL);
	}
	param.cond->f->release(param.cond);
}

bool KSink::response_100_continue() {
	if (!internal_response_status(100)) {
		return false;
	}
	if (internal_start_response_body(0, true) < 0) {
		return false;
	}
	flush();
	return true;
}
bool KSink::parse_header(const char* attr, int attr_len, const char* val, int val_len, bool is_first) {
	//printf("%.*s%s%.*s\n",attr_len, attr, (is_first?" ":": "),val_len, val);
	if (is_first) {
		start_parse();
	}
#if defined(ENABLE_HTTP2) || defined(ENABLE_HTTP3)
	if (data.http_version > 0x100 && *attr == ':') {
		//pseudo header
		if (kgl_mem_same(attr, attr_len, kgl_expand_string(":method"))) {
			if (!data.parse_method(val, val_len)) {
				return false;
			}
			if (data.meth == METH_CONNECT) {
				data.meth = METH_GET;
				KBIT_SET(data.flags, RQ_HAS_CONNECTION_UPGRADE);
			}
			return true;
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
		if (kgl_mem_same(attr, attr_len, _KS(":protocol"))) {
			return data.add_header(kgl_header_upgrade, val, val_len);
		}
		return true;
	}
#endif
	if (is_first && data.http_version < 0x200) {
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
		switch (data.meth) {
		case METH_CONNECT:
			if (!data.parse_connect_url((u_char*)val, url_len)) {
				return false;
			}
		case METH_PRI:
			return true;
		default:
			if (!parse_url(val, url_len, data.raw_url)) {
				return false;
			}
		}
		if (!data.parse_http_version(space, val_len - url_len)) {
			//klog(KLOG_DEBUG, "httpparse:cann't parse http version [%s]\n", space);
			return false;
		}
		if (data.http_version > 0x100) {//data.http_major > 1 || (data.http_major == 1 && data.http_minor == 1)) {
			KBIT_SET(data.flags, RQ_HAS_KEEP_CONNECTION);
		}
		return true;
	}
	switch (*attr) {
	case 'A':
	case 'a':
		if (kgl_mem_case_same(attr, attr_len, kgl_expand_string("Accept-Encoding"))) {
			KHttpFieldValue field(val, val + val_len);
			int64_t q;
			for (;;) {
				const char* field_end = field.get_field_end();
				if (field.get_double_param(_KS("q"), field_end, 3, &q)) {
					if (q == 0) {
						goto next_field;
					}
				}
				if (field.is(kgl_expand_string("gzip"))) {
					KBIT_SET(data.raw_url->accept_encoding, KGL_ENCODING_GZIP);
				} else if (field.is(kgl_expand_string("deflate"))) {
					KBIT_SET(data.raw_url->accept_encoding, KGL_ENCODING_DEFLATE);
				} else if (field.is(kgl_expand_string("compress"))) {
					KBIT_SET(data.raw_url->accept_encoding, KGL_ENCODING_COMPRESS);
				} else if (field.is(kgl_expand_string("br"))) {
					KBIT_SET(data.raw_url->accept_encoding, KGL_ENCODING_BR);
				} else if (field.is(kgl_expand_string("zstd"))) {
					KBIT_SET(data.raw_url->accept_encoding, KGL_ENCODING_ZSTD);
				} else if (!field.is(kgl_expand_string("identity"))) {
					KBIT_SET(data.raw_url->accept_encoding, KGL_ENCODING_UNKNOW);
				}
			next_field:
				if (!field.next(field_end)) {
					break;
				}
			}
			return data.add_header(kgl_header_accept_encoding, val, val_len);
		}
		break;
	case 'C':
	case 'c':
		if (kgl_mem_case_same(attr, attr_len, kgl_expand_string("Connection"))
#ifdef HTTP_PROXY
			|| kgl_mem_case_same(attr, attr_len, kgl_expand_string("proxy-connection"))
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
		if (kgl_mem_case_same(attr, attr_len, kgl_expand_string("Content-length"))) {
			data.left_read = string2int(val);
			data.flags |= RQ_HAS_CONTENT_LEN;
			return true;
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
		break;
	case 'E':
	case 'e':
		if (kgl_mem_case_same(attr, attr_len, kgl_expand_string("Expect"))) {
			if (kgl_memstr(val, val_len, kgl_expand_string("100-continue")) != NULL) {
				data.flags |= RQ_HAVE_EXPECT;
				return data.add_header(kgl_header_expect, val, val_len);
			}
			//unknow expect header.
			return true;
		}
		break;
	case 'H':
	case 'h':
		if (kgl_mem_case_same(attr, attr_len, kgl_expand_string("Host"))) {
			return data.parse_host(val, val_len);
		}
		break;
	case 'I':
	case 'i':
		if (kgl_mem_case_same(attr, attr_len, kgl_expand_string("If-Range"))) {
			time_t try_time = kgl_parse_http_time((u_char*)val, val_len);
			if (try_time == -1) {
				alloc_request_range()->if_range_entity = alloc_entity(val, val_len);
				KBIT_CLR(data.flags, RQ_IF_RANGE_DATE);
			} else {
				alloc_request_range()->if_range_date = try_time;
				KBIT_SET(data.flags, RQ_IF_RANGE_DATE);
			}
			return true;
		}
		if (kgl_mem_case_same(attr, attr_len, kgl_expand_string("If-Modified-Since"))) {
			if (data.precondition.time == 0) {
				data.precondition.time = kgl_parse_http_time((u_char*)val, val_len);
				KBIT_SET(data.flags, RQ_IF_TIME);
			}
			return true;
		}
		if (kgl_mem_case_same(attr, attr_len, kgl_expand_string("If-None-Match"))) {
			data.precondition.entity = alloc_entity(val, val_len);
			KBIT_CLR(data.flags, RQ_IF_TIME | RQ_IF_MATCH_UNMODIFIED);
			return true;
		}
		break;
	case 'P':
	case 'p':
		if (kgl_mem_case_same(attr, attr_len, kgl_expand_string("Pragma"))) {
			if (kgl_memstr(val, val_len, kgl_expand_string("no-cache"))) {
				data.flags |= RQ_HAS_NO_CACHE;
			}
			return data.add_header(kgl_header_pragma, val, val_len);
		}
		break;
	case 'R':
	case 'r':
		if (kgl_mem_case_same(attr, attr_len, _KS("Referer"))) {
			return data.add_header(kgl_header_referer, val, val_len);
		}
		if (kgl_mem_case_same(attr, attr_len, kgl_expand_string("Range"))) {
			if (val_len > 6 && !strncasecmp(val, kgl_expand_string("bytes="))) {
				u_char* end = (u_char*)val + val_len;
				u_char* hot = (u_char*)val + 6;
				kgl_request_range* range = alloc_request_range();
				if (*hot == '-') {
					/* last range model */
					range->from = -kgl_atol(hot + 1, end - hot - 1);
					return true;
				}
				range->from = kgl_atol(hot, end - hot);
				hot = (u_char*)memchr(hot, '-', end - hot);
				if (hot && hot < end - 1) {
					hot++;
					range->to = kgl_atol(hot, end - hot);
				} else {
					/* to end */
					range->to = -1;
				}
				/*
				u_char* next_range = (u_char*)memchr(hot, ',', end - hot);
				if (next_range) {
					//we do not support multi range
					end = next_range;
					return data.add_header(kgl_header_range, val, (int)(end - (u_char*)val), true);
				}
				*/
				return true;
			}
			return data.add_header(kgl_header_range, val, val_len);
		}
		break;
	case 'T':
	case 't':
		if (kgl_mem_case_same(attr, attr_len, kgl_expand_string("Transfer-Encoding"))) {
			if (kgl_mem_case_same(val, val_len, kgl_expand_string("chunked"))) {
				KBIT_SET(data.flags, RQ_INPUT_CHUNKED);
				data.left_read = -1;
			}
			return true;
		}
		break;
	case 'U':
	case 'u':
		if (kgl_mem_case_same(attr, attr_len, _KS("User-Agent"))) {
			return data.add_header(kgl_header_user_agent, val, val_len);
		}
		if (kgl_mem_case_same(attr, attr_len, _KS("Upgrade"))) {
			return data.add_header(kgl_header_upgrade, val, val_len);
		}
		break;
	case 'X':
	case 'x':
		if (kgl_mem_case_same(attr, attr_len, kgl_expand_string("X-Forwarded-Proto"))) {
			if (kgl_mem_case_same(val, val_len, kgl_expand_string("https"))) {
				KBIT_SET(data.raw_url->flags, KGL_URL_ORIG_SSL);
			} else {
				KBIT_CLR(data.raw_url->flags, KGL_URL_ORIG_SSL);
			}
			return true;
		}
		break;
	default:
		break;
	}
	return data.add_header(attr, attr_len, val, val_len);
}
const char* KSink::get_state() {
	switch (data.state) {
	case STATE_IDLE:
		return "idle";
	case STATE_SEND:
		return "send";
	case STATE_RECV:
		return "recv";
	case STATE_WAIT:
		return "wait";
	}
	return "unknow";
}

int KSink::read(char* buf, int len) {
	KBIT_SET(data.flags, RQ_HAS_READ_POST);
	kassert(!kfiber_is_main());
	if (KBIT_TEST(data.flags, RQ_HAVE_EXPECT)) {
		KBIT_CLR(data.flags, RQ_HAVE_EXPECT);
		response_100_continue();
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
		if (data.left_read > 0) {
			assert(data.left_read >= length);
			data.left_read -= length;
		}
		add_up_flow(length);
	}
	return length;
}
bool kgl_init_sink_queue() {
	pthread_key_create(&kgl_request_key, NULL);
	if (0 != selector_manager_thread_init(kgl_request_thread_init, NULL)) {
		klog(KLOG_ERR, "init_http_server_callback must called early!!!\n");
		return false;
	}
	return true;
}
