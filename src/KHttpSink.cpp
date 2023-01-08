#include "KHttpSink.h"
#include "KRequest.h"
#include "kbuf.h"
#include "kfiber.h"
#include "kfeature.h"
#include "khttp.h"
#include "KHttpKeyValue.h"
#include "KHttpServer.h"
#include "KHttp2.h"
#include "klog.h"

#define MAX_HTTP_CHUNK_SIZE 8192
kev_result delete_request_fiber(KOPAQUE data, void* arg, int got) {
	KSink* rq = (KSink*)arg;
	delete rq;
	return kev_ok;
}
kev_result end_http_sink_fiber(KOPAQUE data, void* arg, int got) {
	KHttpSink* sink = static_cast<KHttpSink*>((KSink*)arg);
	sink->EndFiber();
	return kev_ok;
}
kev_result result_skip_chunk_request(KOPAQUE data, void* arg, int got) {
	KSink* s = (KSink*)arg;
	if (got < 0) {
		delete s;
		return kev_destroy;
	}
	KHttpSink* sink = static_cast<KHttpSink*>(s);
	if (got == 0) {
		sink->data.left_read = 0;
		sink->StartPipeLine();
		return kev_ok;
	}
	sink->SkipPost();
	return kev_ok;
}
kev_result result_skip_post(KOPAQUE data, void* arg, int got) {
	KHttpSink* sink = static_cast<KHttpSink*>((KSink*)arg);
	if (got <= 0) {
		delete sink;
		return kev_destroy;
	}
	ks_write_success(&sink->buffer, got);
	sink->SkipPost();
	return kev_ok;
}
kev_result result_read_http_sink(KOPAQUE data, void* arg, int got) {
	KHttpSink* sink = static_cast<KHttpSink*>((KSink*)arg);
	if (got <= 0) {
		delete sink;
		return kev_destroy;
	}
	ks_write_success(&sink->buffer, got);
	return sink->Parse();
}

int buffer_read_http_sink(KOPAQUE data, void* arg, WSABUF* buf, int bufCount) {
	KHttpSink* sink = static_cast<KHttpSink*>((KSink*)arg);
	int bc = ks_get_write_buffers(&sink->buffer, buf, bufCount);
	return bc;
}
#ifdef KSOCKET_SSL
static int handle_http2https_error(void* arg, int got) {
	KSink* sink = (KSink*)arg;
	KBIT_SET(sink->data.flags, RQ_CONNECTION_CLOSE);
	if (http2https_error) {
		http2https_error(sink, 0);
		return 0;
	}
	const char* body = "send http to https port";
	int body_len = (int)strlen(body);
	sink->response_status(STATUS_HTTP_TO_HTTPS);
	sink->response_content_length(body_len);
	sink->response_header(kgl_expand_string("Cache-Control"), kgl_expand_string("no-cache,no-store"));
	sink->response_connection();
	sink->start_response_body(body_len);
	sink->write_all(body, body_len);
	sink->end_request();
	return 0;
}
#endif
KHttpSink::KHttpSink(kconnection* c, kgl_pool_t* pool) : KSingleConnectionSink(c, pool) {
	ks_buffer_init(&buffer, MAX_HTTP_CHUNK_SIZE);
	memset(&parser, 0, sizeof(parser));
	rc = NULL;
	dechunk = NULL;
}
KHttpSink::~KHttpSink() {
	//printf("~KHttpSink\n");
	if (dechunk) {
		delete dechunk;
	}
	if (buffer.buf) {
		xfree(buffer.buf);
	}
}
bool KHttpSink::internal_response_status(uint16_t status_code) {
	kassert(rc);
	kgl_str_t request_line;
	KHttpKeyValue::get_request_line(pool, status_code, &request_line);
	rc->head_insert_const(request_line.data, (uint16_t)request_line.len);
	return true;
}
kev_result KHttpSink::Parse() {
	khttp_parse_result rs;
	char* hot = buffer.buf;
	char* end = buffer.buf + buffer.used;
	for (;;) {
		memset(&rs, 0, sizeof(rs));
		kgl_parse_result result = khttp_parse(&parser, &hot, end, &rs);
		//printf("len=[%d],result=[%d]\n", len,result);

		switch (result) {
		case kgl_parse_continue:
		{
			if (kgl_current_msec - data.begin_time_msec > 60000) {
				delete this;
				return kev_destroy;
			}
			if (parser.header_len > MAX_HTTP_HEAD_SIZE) {
				delete this;
				return kev_destroy;
			}
			ks_save_point(&buffer, hot);
			return read_header();
		}
		case kgl_parse_success:
			if (!parse_header(rs.attr, rs.attr_len, rs.val, rs.val_len, rs.is_first)) {
				delete this;
				return kev_destroy;
			}
			if (rs.is_first && data.meth == METH_PRI && KBIT_TEST(cn->server->flags, KGL_SERVER_H2)) {
#ifdef ENABLE_HTTP2
				ks_save_point(&buffer, hot);
				if (!switch_h2c()) {
					klog(KLOG_ERR, "cann't switch to h2c, buffer size=[%d] may greater than http2 buffer\n", buffer.used);
				}
#endif
				delete this;
				return kev_destroy;
			}
			break;
		case kgl_parse_finished:
			kassert(rc == NULL);
			ksocket_delay(cn->st.fd);
			ks_save_point(&buffer, hot);
			if (KBIT_TEST(data.flags, RQ_INPUT_CHUNKED)) {
				kassert(dechunk == NULL);
				dechunk = new KDechunkContext;
			}
			//printf("***************body_len=[%d]\n", parser.body_len);
			rc = new KResponseContext(pool);
#ifdef KSOCKET_SSL
			if (kconnection_is_ssl_not_handshake(cn)) {
				kfiber_create(handle_http2https_error, (KSink*)this, 0, http_config.fiber_stack_size, NULL);
				return kev_ok;
			}
#endif
			kfiber_create(khttp_server_new_request, (KSink*)this, parser.header_len, http_config.fiber_stack_size, NULL);
			return kev_ok;
		default:
			delete this;
			return kev_destroy;
		}
	}
}
#ifdef ENABLE_HTTP2
bool KHttpSink::switch_h2c() {
	KHttp2* http2 = new KHttp2();
	selectable_bind_opaque(&cn->st, http2);
	if (!http2->server_h2c(cn, buffer.buf, buffer.used)) {
		return false;
	}
	//cn is handle by http2
	cn = nullptr;
	return true;
}
#endif
bool KHttpSink::response_header(kgl_header_type know_header, const char* val, int val_len, bool lock_value) {
	assert(know_header < kgl_header_unknow);
	rc->head_append_const(kgl_header_type_string[know_header].http11.data, (uint16_t)kgl_header_type_string[know_header].http11.len);
	if (lock_value) {
		rc->head_append_const(val, val_len);
	} else {
		char* buf = (char*)kgl_pnalloc(rc->ab.pool, val_len);
		memcpy(buf, val, val_len);
		rc->head_append(buf, (uint16_t)val_len);
	}
	return true;
}
bool KHttpSink::response_header(const char* name, int name_len, const char* val, int val_len) {
	kassert(rc);
	int len = name_len + val_len + 4;
	char* buf = (char*)kgl_pnalloc(rc->ab.pool, len);
	char* hot = buf;
	kgl_memcpy(hot, "\r\n", 2);
	hot += 2;
	kgl_memcpy(hot, name, name_len);
	hot += name_len;
	kgl_memcpy(hot, ": ", 2);
	hot += 2;
	kgl_memcpy(hot, val, val_len);
	hot += val_len;
	rc->head_append(buf, len);
	return true;
}
int KHttpSink::internal_start_response_body(int64_t body_size, bool is_100_continue) {
	if (rc == NULL) {
		return 0;
	}
	send_alt_svc_header();
	rc->head_append_const(_KS("\r\n\r\n"));
	rc->ab.SwitchRead();
	int header_len = rc->ab.getLen();
	assert(!kfiber_is_main());

	WSABUF buf[64];
	for (;;) {
		int bc = rc->ab.getReadBuffer(buf, 64);
		kassert(bc > 0);
		int got = kfiber_net_writev(cn, buf, bc);
		if (got <= 0) {
			header_len = -1;
			break;
		}
		if (!rc->ab.readSuccess(&got)) {
			kassert(got == 0);
			break;
		}
	}
	if (!is_100_continue) {
		delete rc;
		rc = NULL;
	}
	return header_len;
}
int KHttpSink::internal_read(char* buf, int len) {
	if (dechunk) {
		return dechunk->read(this, buf, len);
	}
	if (buffer.used > 0) {
		len = KGL_MIN((int)len, buffer.used);
		kgl_memcpy(buf, buffer.buf, len);
		ks_save_point(&buffer, buffer.buf + len);
		return len;
	}
	return kfiber_net_read(cn, buf, len);
}
int KHttpSink::sendfile(kfiber_file* fp, int len) {
	if (!KBIT_TEST(data.flags, RQ_TE_CHUNKED)) {
		return KSingleConnectionSink::sendfile(fp, len);
	}
	char header[32];
	int size = sprintf(header, "%x\r\n", len);
	if (!kfiber_net_write_full(cn, header, &size)) {
		return -1;
	}
	size = len;
	if (!kfiber_sendfile_full(cn, fp, &size)) {
		return -1;
	}
	size = sizeof("\r\n");
	if (!kfiber_net_write_full(cn, "\r\n", &size)) {
		return -1;
	}
	add_down_flow(len);
	return len;
}
int KHttpSink::internal_write(WSABUF* buf, int bc) {
	assert(!kfiber_is_main());
	if (KBIT_TEST(data.flags, RQ_TE_CHUNKED)) {
		int size = 0;
		for (int i = 0; i < bc; i++) {
			size += buf[i].iov_len;
		}
		char header[32];
		WSABUF* new_bufs = (WSABUF*)alloca(sizeof(WSABUF) * (bc + 2));
		new_bufs[0].iov_len = sprintf(header, "%x\r\n", size);
		new_bufs[0].iov_base = header;
		memcpy(new_bufs + 1, buf, sizeof(WSABUF) * bc);
		bc++;
		new_bufs[bc].iov_base = (char*)"\r\n";
		new_bufs[bc].iov_len = 2;
		bc++;
		if (!kfiber_net_writev_full(cn, new_bufs, &bc)) {
			return -1;
		}
		return size;
	}
	return kfiber_net_writev(cn, buf, bc);
}
int KHttpSink::end_request() {
	if (rc) {
		delete rc;
		rc = NULL;
		KBIT_SET(data.flags, RQ_CONNECTION_CLOSE);
	} else if (KBIT_TEST(data.flags, RQ_TE_CHUNKED | RQ_BODY_NOT_COMPLETE) == RQ_TE_CHUNKED) {
		//has chunked but body is complete successful.
		WSABUF bufs;
		int bc = 1;
		if (!KBIT_TEST(data.flags, RQ_HAS_SEND_CHUNK_0)) {
			KBIT_SET(data.flags, RQ_HAS_SEND_CHUNK_0);
			bufs.iov_base = (char*)"0\r\n\r\n";
			bufs.iov_len = 5;
		} else {
			bufs.iov_base = (char*)"\r\n";
			bufs.iov_len = 2;
		}
		if (!kfiber_net_writev_full(cn, &bufs, &bc)) {
			KBIT_SET(data.flags, RQ_CONNECTION_CLOSE);
		}
	}
	if (KBIT_TEST(data.flags, RQ_CONNECTION_CLOSE | RQ_CONNECTION_UPGRADE) || !KBIT_TEST(data.flags, RQ_HAS_KEEP_CONNECTION)) {
		return kfiber_exit_callback(NULL, delete_request_fiber, (KSink*)this);
	}
	ksocket_no_delay(cn->st.fd, false);
	kassert(buffer.buf_size > 0);
	kassert(data.left_read >= 0 || dechunk != NULL);

	if (data.left_read != 0 && !KBIT_TEST(data.flags, RQ_HAVE_EXPECT)) {
		//still have data to read
		SkipPost();
		return 0;
	}
	return StartPipeLine();
}
void KHttpSink::SkipPost() {
	kassert(data.left_read != 0);
	if (dechunk) {
		for (;;) {
			int got = dechunk->read(this, NULL, 8192);
			if (got < 0) {
				kfiber_exit_callback(NULL, delete_request_fiber, (KSink*)this);
				return;
			}
			if (got == 0) {
				data.left_read = 0;
				StartPipeLine();
				return;
			}
			add_up_flow((INT64)got);
		}
		return;
	}
	if (data.left_read <= 0) {
		kfiber_exit_callback(NULL, delete_request_fiber, (KSink*)this);
		return;
	}
	int buf_size;
	char* buf = ks_get_write_buffer(&buffer, &buf_size);
	int skip_len = (int)(KGL_MIN(data.left_read, (INT64)buffer.used));
	ks_save_point(&buffer, buffer.buf + skip_len);
	data.left_read -= skip_len;
	add_up_flow((INT64)skip_len);
	if (data.left_read <= 0) {
		StartPipeLine();
		return;
	}
	while (data.left_read > 0) {
		int len = kfiber_net_read(cn, buf, (int)KGL_MIN((int64_t)buf_size, data.left_read));
		if (len <= 0) {
			kfiber_exit_callback(NULL, delete_request_fiber, (KSink*)this);
			return;
		}
		data.left_read -= len;
	}
	StartPipeLine();
}
void KHttpSink::EndFiber() {
	if (buffer.used > 0) {
		Parse();
		return;
	}
	read_header();
}
int KHttpSink::StartPipeLine() {
	kassert(data.left_read == 0 || KBIT_TEST(data.flags, RQ_HAVE_EXPECT));
	reset_pipeline();
	memset(&parser, 0, sizeof(parser));
	if (dechunk) {
		delete dechunk;
		dechunk = NULL;
	}
	return kfiber_exit_callback(NULL, end_http_sink_fiber, (KSink*)this);
}
kev_result KHttpSink::read_header() {
	return selectable_read(&cn->st, result_read_http_sink, buffer_read_http_sink, (KSink*)this);
}
bool KHttpSink::response_trailer(const char* name, int name_len, const char* val, int val_len) {
	if (!KBIT_TEST(data.flags, RQ_TE_CHUNKED)) {
		return true;
	}
	WSABUF bufs[5];
	int bc = 0;
	if (!KBIT_TEST(data.flags, RQ_HAS_SEND_CHUNK_0)) {
		KBIT_SET(data.flags, RQ_HAS_SEND_CHUNK_0);
		bufs[bc].iov_base = (char*)"0\r\n";
		bufs[bc].iov_len = 3;
		bc++;
	}
	bufs[bc].iov_base = (char*)name;
	bufs[bc].iov_len = name_len;
	bc++;
	bufs[bc].iov_base = (char*)": ";
	bufs[bc].iov_len = 2;
	bc++;
	bufs[bc].iov_base = (char*)val;
	bufs[bc].iov_len = val_len;
	bc++;
	bufs[bc].iov_base = (char*)"\r\n";
	bufs[bc].iov_len = 2;
	bc++;
	return kfiber_net_writev_full(cn, bufs, &bc);
}