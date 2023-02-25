#include "KWebDavRequest.h"
#include "KWebDavClient.h"
#include "KStringBuf.h"

bool webdav_header_callback(KUpstream* us, void* arg, const char* attr, int attr_len, const char* val, int val_len, bool is_first)
{
	//printf("%s%s%s\n", attr, is_first ? " " : ": ", val);
	KWebDavRequest* rq = (KWebDavRequest*)arg;
	return rq->resp.parse_header(us, attr, attr_len, val, val_len, is_first);
}
KWebDavRequest::KWebDavRequest(KWebDavClient* client, KUpstream* us)
{
	this->client = client;
	this->resp.us = us;
}
KWebDavRequest::~KWebDavRequest()
{
	//printf("destroy webdav request=[%p]\n", this);
	//assert(resp.left == 0);
	
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
	return resp.read_body(result, 4096578);
}
KGL_RESULT KWebDavRequest::read_body(KXmlDocument& body)
{
	KGL_RESULT result;
	ks_buffer* buffer = read_body(result);
	if (buffer == nullptr) {
		return result;
	}
	//printf("[%s]\n", buffer->buf);
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
	return resp.us->send_header(kgl_expand_string("If"), s.buf(), s.size());
}
bool KWebDavRequest::send_lock_token(KWebDavLockToken* token)
{
	KStringBuf s;
	s << "<" << token->token << ">";
	return resp.us->send_header(kgl_expand_string("Lock-Token"), s.buf(), s.size());
}
bool KWebDavRequest::send_http_auth(KWebDavAuth* auth)
{
	KStringBuf s;
	s << "Basic ";
	KStringBuf as;
	as << auth->user.c_str() << ":" << auth->passwd.c_str();
	s << b64encode((unsigned char*)as.buf(), as.size());
	return resp.us->send_header(kgl_expand_string("Authorization"), s.buf(), s.size());
}