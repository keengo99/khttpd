#include "KWebDavRequest.h"
#include "KWebDavClient.h"
#include "KStringBuf.h"

bool webdav_header_callback(KUpstream* us, void* arg, const char* attr, int attr_len, const char* val, int val_len, bool is_first)
{
	//printf("%s%s%s\n", attr, is_first ? " " : ": ", val);
	KWebDavRequest* rq = (KWebDavRequest*)arg;
	if (is_first) {
		rq->resp.status_code = atoi(val);
		if (is_status_code_no_body(rq->resp.status_code)) {
			rq->resp.left = 0;
		}
		return true;
	}
	if (strcasecmp(attr, "Transfer-Encoding") == 0) {
		if (strcasecmp(val, "chunked") == 0 && rq->dechunk==nullptr) {
			rq->resp.left = -1;
			rq->dechunk = new KDechunkReader<KUpstream>(us);
		}
		return true;
	}
	if (!strcasecmp(attr, "Content-length")) {
		rq->resp.left = string2int(val);
		if (rq->dechunk) {
			delete rq->dechunk;
			rq->dechunk = nullptr;
		}
		return true;
	}
	rq->resp.AddHeader(attr, attr_len, val, val_len);
	return true;
}
KWebDavRequest::KWebDavRequest(KWebDavClient* client, KUpstream* us)
{
	this->client = client;
	this->us = us;
	dechunk = nullptr;
	memset(&resp, 0, sizeof(resp));
}
KWebDavRequest::~KWebDavRequest()
{
	//printf("destroy webdav request=[%p]\n", this);
	assert(resp.left == 0);
	this->us->gc(resp.left==0?10:-1);
	if (resp.header) {
		free_header_list(resp.header);
	}
	if (dechunk) {
		delete dechunk;
	}
}
KGL_RESULT KWebDavRequest::skip_body()
{
	for (;;) {
		char buf[512];
		int len = read(buf, sizeof(buf));
		if (len == 0) {
			return KGL_OK;
		}
		if (len < 0) {
			return KGL_ESOCKET_BROKEN;
		}
		//fwrite(buf, 1, len, stdout);
	}

}
ks_buffer *KWebDavRequest::read_body(KGL_RESULT &result)
{
	int64_t body_len = get_left();
	if (body_len == 0) {
		result = KGL_OK;
		return nullptr;
	}
	if (body_len > 4096578) {
		result = KGL_EINSUFFICIENT_BUFFER;
		return nullptr;
	}
	if (body_len > 0) {
		ks_buffer* buffer = ks_buffer_new((int)(body_len+1));
		if (!read_all(buffer->buf, (int)body_len)) {
			ks_buffer_destroy(buffer);
			result = KGL_ESOCKET_BROKEN;
			return nullptr;
		}
		result = KGL_OK;
		return buffer;
	}
	ks_buffer* buffer = ks_buffer_new(4096);
	for (;;) {
		int len;
		char* buf = ks_get_write_buffer(buffer, &len);
		//printf("before us_read buf=[%p] len=[%d]\n",buf, len);
		int got = read(buf,len);
		assert(got <= len);
		//printf("got=[%d]\n", got);
		if (got < 0) {
			ks_buffer_destroy(buffer);
			result = KGL_ESOCKET_BROKEN;
			return nullptr;
		}
		if (got == 0) {
			ks_write_str(buffer, "\0", 1);
			return buffer;
		}
		ks_write_success(buffer, got);
	}
	return nullptr;
}
KGL_RESULT KWebDavRequest::read_body(KXmlDocument& body)
{
	KGL_RESULT result;
	ks_buffer* buffer = read_body(result);
	if (buffer == nullptr) {
		return result;
	}
	//printf("%s\n", buffer->buf);
	body.parse(buffer->buf);
	ks_buffer_destroy(buffer);
	return KGL_OK;
}
bool KWebDavRequest::send_if_lock_token(KWebDavLockToken* token, bool send_resource)
{
	KStringBuf s;
	if (send_resource) {
		s << "<";
		client->url->GetSchema(s);
		client->url->GetHost(s);
		s << token->path;
		s << "> ";
	}
	s << "(<" << token->token << ">)";
	return us->send_header(kgl_expand_string("If"), s.getBuf(), s.getSize());
}
bool KWebDavRequest::send_lock_token(KWebDavLockToken* token)
{
	KStringBuf s;
	s << "<" << token->token << ">";
	return us->send_header(kgl_expand_string("Lock-Token"), s.getBuf(), s.getSize());
}
bool KWebDavRequest::send_http_auth(KWebDavAuth* auth)
{
	KStringBuf s;
	s << "Basic ";
	KStringBuf as;
	as << auth->user.c_str() << ":" << auth->passwd.c_str();
	s << b64encode((unsigned char*)as.getBuf(), as.getSize());
	return us->send_header(kgl_expand_string("Authorization"), s.getBuf(), s.getSize());
}