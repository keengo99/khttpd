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
	if (rc) {
		delete rc;
	}
}
bool KHttpSink::internal_response_status(uint16_t status_code) {
	kassert(rc);
	kgl_str_t request_line;
	KHttpKeyValue::get_request_line(pool, status_code, &request_line);
	rc->head_insert_const(request_line.data, (uint16_t)request_line.len);
	return true;
}
#ifdef ENABLE_HTTP2
static kev_result h2c_process(KOPAQUE data, void* arg, int got) {
	KHttp2* http2 = (KHttp2*)arg;
	http2->server_h2c(got);
	return kev_ok;
}
bool KHttpSink::switch_h2c() {
	KHttp2* http2 = new KHttp2();
	selectable_bind_opaque(&cn->st, http2);
	if (!http2->init_h2c(cn, buffer.buf, buffer.used)) {
		return false;
	}
	selectable_next(&cn->st, h2c_process, http2, buffer.used);
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
	this->response_left = body_size;
	send_alt_svc_header();
	rc->head_append_const(_KS("\r\n\r\n"));
	rc->ab.SwitchRead();
	int header_len = rc->ab.getLen();
	assert(!kfiber_is_main());
	if (KBIT_TEST(data.flags, RQ_CONNECTION_UPGRADE)) {
		if (0 != KSingleConnectionSink::write_all(rc->ab.getHead(), rc->ab.getLen())) {
			return -1;
		}
		delete rc;
		rc = nullptr;
		return header_len;
	}
	if (is_100_continue) {
		if (0 != KSingleConnectionSink::write_all(rc->ab.getHead(), rc->ab.getLen())) {
			return -1;
		}
		rc->ab.clean();
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
	if (rc && !rc->ab.empty()) {
		int left = KSingleConnectionSink::write_all(rc->ab.getHead(), rc->ab.getLen());
		delete rc;
		rc = nullptr;
		if (left != 0) {
			return 0;
		}
	}
	if (!KBIT_TEST(data.flags, RQ_TE_CHUNKED)) {
		return KSingleConnectionSink::sendfile(fp, len);
	}
	char header[32];
	int size = sprintf(header, "%x\r\n", len);
	if (!kfiber_net_write_full(cn, header, &size)) {
		return -1;
	}
	int size2 = len;
	bool result = kfiber_sendfile_full(cn, fp, &size2);
	add_down_flow(len - size2 + size + 2);
	if (!result) {
		return -1;
	}
	size2 = sizeof("\r\n");
	if (!kfiber_net_write_full(cn, "\r\n", &size2)) {
		return -1;
	}
	on_success_response(len+size+2);
	return len;
}
int KHttpSink::write_all(const char* str, int len) {
	if (KBIT_TEST(data.flags, RQ_TE_CHUNKED)) {
		char header[32];
		kbuf buf[2];
		buf[0].used = sprintf(header, "%x\r\n", len);
		buf[0].next = &buf[1];
		buf[0].data = (char*)header;
		buf[1].used = len;
		buf[1].data = (char*)str;
		kgl_iovec chunked_end;
		chunked_end.iov_base = (char*)"\r\n";
		chunked_end.iov_len = 2;
		return internal_write(buf, len + buf[0].used, &chunked_end);
	}
	if (rc && !rc->ab.empty()) {
		kbuf buf{ 0 };
		buf.data = (char*)str;
		buf.used = len;
		return internal_write(&buf, len, nullptr);
	}
	return KSingleConnectionSink::write_all(str, len);
}
int KHttpSink::write_all(const kbuf* buf, int len) {
	if (KBIT_TEST(data.flags, RQ_TE_CHUNKED)) {
		char header[32];
		kbuf chunked_buf;
		chunked_buf.used = sprintf(header, "%x\r\n", len);
		chunked_buf.next = (kbuf *)buf;
		chunked_buf.data = (char*)header;
		kgl_iovec chunked_end;
		chunked_end.iov_base = (char*)"\r\n";
		chunked_end.iov_len = 2;
		return internal_write(&chunked_buf, len + chunked_buf.used, &chunked_end);
	}
	return internal_write(buf, len, nullptr);
}
void KHttpSink::end_request() {
	if (rc) {
		if (rc->ab.empty()) {
			KBIT_SET(data.flags, RQ_CONNECTION_CLOSE);
			return;
		}
		//has header to send.
		if (KSingleConnectionSink::write_all(rc->ab.getHead(), rc->ab.getLen()) != 0) {
			KBIT_SET(data.flags, RQ_CONNECTION_CLOSE);
			return;
		}
		delete rc;
		rc = nullptr;
	}
	if (response_left > 0) {
		KBIT_SET(data.flags, RQ_CONNECTION_CLOSE);
		return;
	}
	if (KBIT_TEST(data.flags, RQ_TE_CHUNKED) && response_left == -1) {
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
	return;
}
bool KHttpSink::skip_post() {
	kassert(data.left_read != 0);
	if (dechunk) {
		for (;;) {
			int got = dechunk->read(this, NULL, 8192);
			if (got < 0) {
				break;
			}
			if (got == 0) {
				data.left_read = 0;
				return start_pipe_line();
			}
			add_up_flow((INT64)got);
		}
		return false;
	}
	if (data.left_read <= 0) {
		return false;
	}
	int buf_size;
	char* buf = ks_get_write_buffer(&buffer, &buf_size);
	int skip_len = (int)(KGL_MIN(data.left_read, (INT64)buffer.used));
	ks_save_point(&buffer, buffer.buf + skip_len);
	data.left_read -= skip_len;
	add_up_flow((INT64)skip_len);
	if (data.left_read <= 0) {
		return start_pipe_line();
	}
	while (data.left_read > 0) {
		int len = kfiber_net_read(cn, buf, (int)KGL_MIN((int64_t)buf_size, data.left_read));
		if (len <= 0) {
			return false;
		}
		data.left_read -= len;
	}
	return start_pipe_line();
}
bool KHttpSink::start_pipe_line() {
	if (KBIT_TEST(data.flags, RQ_CONNECTION_CLOSE | RQ_CONNECTION_UPGRADE) || !KBIT_TEST(data.flags, RQ_HAS_KEEP_CONNECTION)) {
		return false;
	}
	assert(rc==nullptr);
	//ksocket_no_delay(cn->st.fd, false);
	kassert(buffer.buf_size > 0);
	kassert(data.left_read >= 0 || dechunk != NULL);

	if (data.left_read != 0 && !KBIT_TEST(data.flags, RQ_HAVE_EXPECT)) {
		//still have data to read
		return skip_post();
	}
	kassert(data.left_read == 0 || KBIT_TEST(data.flags, RQ_HAVE_EXPECT));
	reset_pipeline();
	memset(&parser, 0, sizeof(parser));
	if (dechunk) {
		delete dechunk;
		dechunk = NULL;
	}
	return true;
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
void KHttpSink::start(int header_len) {
	assert(header_len == 0);
	for (;;) {
		int len;
		char* hot = ks_get_write_buffer(&buffer, &len);
		int got = kfiber_net_read(cn, hot, len);
		if (got <= 0) {
			return;
		}
		ks_write_success(&buffer, got);
	parse_header:
		switch (parse()) {
		case kgl_parse_continue:
			break;
		case kgl_parse_finished:
#ifdef KSOCKET_SSL
			if (kconnection_is_ssl_not_handshake(cn)) {
				handle_http2https_error((KSink*)this, 0);
				return;
			}
#endif
#ifdef KGL_DEBUG_TIME
			reset_start_time();
#endif
			khttp_server_new_request((KSink*)this, parser.header_len);
#ifdef KGL_DEBUG_TIME
			log_passed_time("+end_request");
#endif
			if (!start_pipe_line()) {
				return;
			}
			if (buffer.used > 0) {
				goto parse_header;
			}
			break;
		default:
			return;
		}
	}
}