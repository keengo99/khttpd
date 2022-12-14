#include "KHttpSink.h"
#include "KRequest.h"
#include "kbuf.h"
#include "kfiber.h"
#include "kfeature.h"
#include "khttp.h"
#include "KHttpKeyValue.h"
#include "KHttpServer.h"

#define MAX_HTTP_CHUNK_SIZE 8192
kev_result delete_request_fiber(KOPAQUE data, void* arg, int got)
{
	KSink* rq = (KSink*)arg;
	delete rq;
	return kev_ok;
}
kev_result end_http_sink_fiber(KOPAQUE data, void* arg, int got)
{
	KHttpSink *sink = static_cast<KHttpSink*>((KSink *)arg);
	sink->EndFiber();
	return kev_ok;
}
static int buffer_write_http_sink(KOPAQUE data, void *arg, LPWSABUF buf, int bc)
{
	KHttpSink *sink = (KHttpSink *)arg;
	kassert(sink->rc);
	return sink->rc->GetReadBuffer(data, buf, bc);
}
static void result_write_http_header(KOPAQUE data, void *arg, int got)
{
	KHttpSink *sink = static_cast<KHttpSink *>((KSink *)arg);
	kassert(sink->rc==NULL);
	if (got < 0) {
		KBIT_SET(sink->data.flags, RQ_CONNECTION_CLOSE);
	}
	sink->end_request();
}
static kev_result result_write_http_sink(KOPAQUE data, void *arg, int got)
{
	KHttpSink *sink = (KHttpSink *)arg;
	KResponseContext *rc = sink->rc;
	kassert(rc && rc->result);
	if (got <= 0) {
		return sink->ResultResponseContext(got);		
	}
	if (rc->ReadSuccess(&got)) {
		kassert(got == 0);
		return selectable_write(&sink->cn->st,result_write_http_sink, buffer_write_http_sink,arg);
	}
	return sink->ResultResponseContext(got);
}
kev_result result_skip_chunk_request(KOPAQUE data, void *arg, int got)
{
	KSink *s = (KSink *)arg;
	if (got < 0) {
		delete s;
		return kev_destroy;
	}
	KHttpSink *sink = static_cast<KHttpSink *>(s);
	if (got == 0) {
		sink->data.left_read = 0;
		sink->StartPipeLine();
		return kev_ok;
	}
	sink->SkipPost();
	return kev_ok;
}
kev_result result_skip_post(KOPAQUE data, void *arg, int got)
{	
	KHttpSink *sink = static_cast<KHttpSink *>((KSink *)arg);
	if (got <= 0) {
		delete sink;
		return kev_destroy;
	}
	ks_write_success(&sink->buffer, got);
	sink->SkipPost();
	return kev_ok;
}
kev_result result_read_http_sink(KOPAQUE data, void *arg, int got)
{
	KHttpSink* sink = static_cast<KHttpSink*>((KSink*)arg);
	if (got <= 0) {
		delete sink;
		return kev_destroy;
	}
	ks_write_success(&sink->buffer, got);
	return sink->Parse();
}

int buffer_read_http_sink(KOPAQUE data, void *arg, LPWSABUF buf, int bufCount)
{
	KHttpSink *sink = static_cast<KHttpSink *>((KSink *)arg);
	int bc = ks_get_write_buffers(&sink->buffer, buf, bufCount);
	return bc;
}
#ifdef KSOCKET_SSL
static int handle_http2https_error(void *arg, int got)
{
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
kev_result KHttpSink::ResultResponseContext(int got)
{
	buffer_callback buffer = rc->buffer;
	result_callback result = rc->result;
	void *arg = rc->arg;
	kassert(got < 0 || rc->GetLen() == 0);
	delete rc;
	rc = NULL;
	if (got==0 && buffer) {
		return selectable_write(&cn->st,result, buffer, arg);
	}
	return result(cn->st.data, arg, got);
}
KHttpSink::KHttpSink(kconnection *c,kgl_pool_t *pool) : KTcpServerSink(pool)
{
	this->cn = c;
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
	kconnection_destroy(cn);
}
void KHttpSink::start_header()
{
	if (rc == NULL) {
		rc = new KResponseContext(pool);
	}
}
bool KHttpSink::internal_response_status(uint16_t status_code)
{
	kassert(rc);
	kgl_str_t request_line;
	getRequestLine(pool, status_code, &request_line);
	rc->head_insert_const(request_line.data, (uint16_t)request_line.len);
	return true;
}
kev_result KHttpSink::Parse()
{
	khttp_parse_result rs;
	char *hot = buffer.buf;
	int len = buffer.used;
	//fwrite(hot, 1, len, stdout);
	for (;;) {
		memset(&rs, 0, sizeof(rs));
		kgl_parse_result result = khttp_parse(&parser, &hot, &len, &rs);
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
			ks_save_point(&buffer, hot, len);	
			return ReadHeader();
		}
		case kgl_parse_success:
			if (!parse_header(rs.attr, rs.attr_len, rs.val, rs.val_len, rs.is_first)) {
				delete this;
				return kev_destroy;
			}
			break;
		case kgl_parse_finished:
			kassert(rc == NULL);
			ksocket_delay(cn->st.fd);
			ks_save_point(&buffer, hot, len);
			if (KBIT_TEST(data.flags, RQ_INPUT_CHUNKED)) {
				kassert(dechunk == NULL);
				dechunk = new KDechunkContext;
			}
			//printf("***************body_len=[%d]\n", parser.body_len);
			rc = new KResponseContext(pool);
#ifdef KSOCKET_SSL
			if (kconnection_is_ssl_not_handshake(cn)) {
				kfiber_create(handle_http2https_error, (KSink *)this, 0, http_config.fiber_stack_size, NULL);
				return kev_ok;
			}
#endif
			kfiber_create(khttp_server_new_request, (KSink *)this, parser.header_len, http_config.fiber_stack_size, NULL);
			return kev_ok;
		default:
			delete this;
			return kev_destroy;
		}
	}
}
bool KHttpSink::response_header(const char *name, int name_len, const char *val, int val_len)
{
	kassert(rc);
	int len = name_len + val_len + 4;
	char *buf = (char *)kgl_pnalloc(rc->ab.pool,len);
	char *hot = buf;
	kgl_memcpy(hot, name, name_len);
	hot += name_len;
	kgl_memcpy(hot, ": ", 2);
	hot += 2;
	kgl_memcpy(hot, val, val_len);
	hot += val_len;
	kgl_memcpy(hot, "\r\n", 2);
	rc->head_append(buf, len);
	return true;
}
int KHttpSink::internal_start_response_body(int64_t body_size)
{
	if (rc == NULL) {
		return 0;
	}
	if (!KBIT_TEST(data.flags, RQ_CONNECTION_UPGRADE)) {
		char buf[32];
		int len = kgl_get_alt_svc_value(cn->server, buf, sizeof(buf));
		if (len > 0) {
			response_header(kgl_expand_string("Alt-Svc"), buf, len);
		}
	}
	rc->head_append_const("\r\n", 2);
	rc->SwitchRead();
	int header_len = rc->GetLen();
	if (!kfiber_is_main()) {
		WSABUF buf[64];
		for (;;) {
			int bc = rc->GetReadBuffer(cn->st.data, buf, 64);
			kassert(bc > 0);
			int got = kfiber_net_writev(cn, buf, bc);
			if (got <= 0) {
				header_len = -1;
				break;
			}
			if (!rc->ReadSuccess(&got)) {
				kassert(got == 0);
				break;
			}
		}
		delete rc;
		rc = NULL;
	}
	return header_len;
}
int KHttpSink::internal_read(char *buf, int len)
{	
	if (dechunk) {
		return dechunk->Read(this, buf, len);
	}
	if (buffer.used > 0) {
		len = MIN((int)len, buffer.used);
		kgl_memcpy(buf, buffer.buf, len);
		ks_save_point(&buffer, buffer.buf + len, buffer.used - len);
		return len;
	}
	return kfiber_net_read(cn, buf, len);
}
int KHttpSink::internal_write(LPWSABUF buf, int bc)
{
	assert(!kfiber_is_main());
	return kfiber_net_writev(cn, buf, bc);
}
int KHttpSink::end_request()
{
	if (rc) {
		delete rc;
		rc = NULL;
		KBIT_SET(data.flags, RQ_CONNECTION_CLOSE);
	}
	if (KBIT_TEST(data.flags, RQ_CONNECTION_CLOSE) || !KBIT_TEST(data.flags, RQ_HAS_KEEP_CONNECTION)) {
		return kfiber_exit_callback(NULL, delete_request_fiber, (KSink *)this);
	}
	ksocket_no_delay(cn->st.fd,false);
	kassert(buffer.buf_size > 0);
	kassert(data.left_read >= 0 || dechunk!=NULL);

	if (data.left_read != 0 && !KBIT_TEST(data.flags, RQ_HAVE_EXPECT)) {
		//still have data to read
		SkipPost();
		return 0;
	}
	return StartPipeLine();
}
void KHttpSink::SkipPost()
{
	kassert(data.left_read != 0);
	if (dechunk) {
		for (;;) {
			int got = dechunk->Read(this, NULL, 8192);
			if (got < 0) {
				kfiber_exit_callback(NULL, delete_request_fiber, (KSink *)this);
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
	char *buf = ks_get_write_buffer(&buffer, &buf_size);
	int skip_len = (int)(MIN(data.left_read, (INT64)buffer.used));
	ks_save_point(&buffer, buffer.buf + skip_len, buffer.used - skip_len);
	data.left_read -= skip_len;
	add_up_flow((INT64)skip_len);
	if (data.left_read <= 0) {
		StartPipeLine();
		return;
	}
	while (data.left_read > 0) {
		int len = kfiber_net_read(cn, buf, (int)MIN((int64_t)buf_size, data.left_read));
		if (len <= 0) {
			kfiber_exit_callback(NULL, delete_request_fiber, (KSink *)this);
			return;
		}
		data.left_read -= len;
	}
	StartPipeLine();
}
void KHttpSink::EndFiber()
{
	if (buffer.used > 0) {
		Parse();
		return;
	}
	ReadHeader();
}
int KHttpSink::StartPipeLine()
{
	kassert(data.left_read == 0 || KBIT_TEST(data.flags, RQ_HAVE_EXPECT));
	reset_pipeline();
	memset(&parser, 0, sizeof(parser));
	if (dechunk) {
		delete dechunk;
		dechunk = NULL;
	}
	return kfiber_exit_callback(NULL, end_http_sink_fiber, (KSink*)this);
}
kev_result KHttpSink::ReadHeader()
{
	return selectable_read(&cn->st, result_read_http_sink, buffer_read_http_sink, (KSink*)this);
}
