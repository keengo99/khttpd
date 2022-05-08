#include "KWebDavRequest.h"
#include "KWebDavClient.h"
#include "KStringBuf.h"

bool webdav_header_callback(KUpstream* us, void* arg, const char* attr, int attr_len, const char* val, int val_len, bool is_first)
{
	//printf("%s%s%s\n", attr, is_first ? " " : ": ", val);
	KWebDavRequest* rq = (KWebDavRequest*)arg;
	if (is_first) {
		rq->resp.status_code = atoi(val);
		return true;
	}
	if (strcasecmp(attr, "Content-Length") == 0) {
		rq->resp.content_length = string2int(val);
		rq->resp.left = rq->resp.content_length;
		return true;
	}
	rq->resp.AddHeader(attr, attr_len, val, val_len);
	return true;
}
KWebDavRequest::KWebDavRequest(KWebDavClient* client, KUpstream* us)
{
	this->client = client;
	this->us = us;
	memset(&resp, 0, sizeof(resp));
}
KWebDavRequest::~KWebDavRequest()
{
	this->us->gc(-1, 0);
	if (resp.header) {
		free_header_list(resp.header);
	}
}
KGL_RESULT KWebDavRequest::skip_body()
{
	if (resp.content_length > 0) {
		char buf[512];
		while (resp.left > 0) {
			int len = read(buf, sizeof(buf));
			if (len <= 0) {
				return KGL_ESOCKET_BROKEN;
			}
			//fwrite(buf, 1, len, stdout);
		}
	}
	return KGL_OK;
}
KGL_RESULT KWebDavRequest::read_body(KXmlDocument& body)
{
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