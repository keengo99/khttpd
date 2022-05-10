#include <sstream>
#include "KWebDavClient.h"
#include "KDefer.h"
#include "KHttpKeyValue.h"
#include "KWebDavFileList.h"


KWebDavClient::KWebDavClient()
{
	url = nullptr;
	sock_pool = nullptr;
	current_lock_token = nullptr;
	auth = nullptr;
}
void KWebDavClient::set_auth(const char* user, const char* passwd) {
	if (auth) {
		delete auth;
	}
	auth = new KWebDavAuth;
	auth->user = user;
	auth->passwd = passwd;
}
bool KWebDavClient::set_url(const char* url)
{
	if (this->url) {
		this->url->relase();
	}
	this->url = new KUrl;
	if (this->sock_pool) {
		this->sock_pool->release();
		this->sock_pool = NULL;
	}
	if (!parse_url(url, this->url)) {
		this->url->relase();
		this->url = NULL;
		return false;
	}
	sock_pool = new KSockPoolHelper();
	sock_pool->setLifeTime(30);
	std::stringstream ports;
	ports << this->url->port;
	if (KBIT_TEST(this->url->flags, KGL_URL_ORIG_SSL)) {
		ports << "sp";
	}
	sock_pool->setHostPort(this->url->host, ports.str().c_str());
	return true;
}
KWebDavClient::~KWebDavClient()
{
	if (url) {
		url->relase();
	}
	if (sock_pool) {
		sock_pool->release();
	}
	if (current_lock_token) {
		delete current_lock_token;
	}
	if (auth) {
		delete auth;
	}
}
KGL_RESULT KWebDavClient::new_request(const char* method, const char* path, int64_t content_length, KWebDavRequest** rq)
{
	if (sock_pool == NULL) {
		return KGL_ENOT_PREPARE;
	}
	char tmpbuff[50];
	KUpstream* us = sock_pool->get_upstream(0);
	if (us == nullptr) {
		return KGL_EIO;
	}
	*rq = new KWebDavRequest(this, us);
	us->set_header_callback(*rq, webdav_header_callback);
	if (!us->send_method_path(KHttpKeyValue::getMethod(method), path, (hlen_t)strlen(path))) {
		delete (*rq);
		*rq = nullptr;
		return KGL_ESOCKET_BROKEN;
	}
	KStringBuf host;
	url->GetHost(host);
	us->send_host(host.getBuf(), host.getSize());
	if (us->IsMultiStream()) {
		us->send_header(kgl_expand_string(":scheme"), kgl_expand_string("https"));
	}
	if (content_length > 0) {
		int len = int2string2(content_length, tmpbuff);
		us->send_header(kgl_expand_string("Content-Length"), tmpbuff, len);
	}
	//us->send_header(kgl_expand_string("Accept-Encoding"), kgl_expand_string("gzip"));
	if (auth) {
		(*rq)->send_http_auth(auth);
	}
	us->set_content_length(content_length);
	return KGL_OK;
}
KGL_RESULT KWebDavClient::option(const char* path)
{
	KWebDavRequest* rq = NULL;
	auto result = new_request("OPTIONS", path, 0, &rq);
	if (rq == nullptr) {
		//printf("webdav new_request failed error=[%d]\n", result);
		return result;
	}
	defer(delete rq);
	rq->send_header_complete();
	result = rq->read_header();
	if (result != KGL_OK) {
		return result;
	}
	if (rq->resp.status_code != STATUS_OK) {
		return KGL_EIO;
	}
	return KGL_OK;
}
KGL_RESULT KWebDavClient::_delete(const char* path)
{
	KWebDavRequest* rq = NULL;
	int64_t content_length = 0;
	auto result = new_request("DELETE", path, 0, &rq);
	if (rq == nullptr) {
		//printf("webdav new_request failed error=[%d]\n", result);
		return result;
	}
	defer(delete rq);
	rq->send_header_complete();
	result = rq->read_header();
	rq->skip_body();
	if (result != KGL_OK) {
		return result;
	}
	//printf("status_code=[%d]\n", rq->resp.status_code);
	if (rq->resp.status_code != STATUS_NO_CONTENT) {
		return KGL_EDATA_FORMAT;
	}
	return KGL_OK;
}
KGL_RESULT KWebDavClient::lock(const char* path, const char* owner)
{
#define LOCK_BODY1 "<?xml version=\"1.0\" encoding=\"utf-8\" ?>\
<D:lockinfo xmlns:D=\"DAV:\"><D:lockscope><D:exclusive/></D:lockscope>\
<D:locktype><D:write/></D:locktype><D:owner><D:href>"

#define LOCK_BODY2 "</D:href></D:owner></D:lockinfo>"
	if (current_lock_token != nullptr) {
		return KGL_EDENIED;
	}
	KStringBuf body;
	body.write_all(kgl_expand_string(LOCK_BODY1));
	if (owner) {
		body.write_all(owner, (int)strlen(owner));
	} else {
		body.write_all(kgl_expand_string("kwebdav_client"));
	}
	body.write_all(kgl_expand_string(LOCK_BODY2));
	int64_t content_length = body.getSize();
	KWebDavRequest* rq = NULL;
	auto result = new_request("LOCK", path, content_length, &rq);
	if (rq == nullptr) {
		printf("webdav new_request failed error=[%d]\n", result);
		return result;
	}
	defer(delete rq);
	rq->send_header_complete();
	rq->write_all(body.getBuf(), body.getSize());
	result = rq->read_header();
	if (result != KGL_OK) {
		return result;
	}
	if (rq->resp.status_code != STATUS_OK) {
		return KGL_EDATA_FORMAT;
	}
	KHttpHeader* header = rq->resp.FindHeader(kgl_expand_string("Lock-Token"));
	if (header == NULL) {
		return KGL_EDATA_FORMAT;
	}
	u_char* pos = (u_char*)strchr(header->val, '<');
	if (pos == NULL) {
		return KGL_EDATA_FORMAT;
	}
	pos++;
	u_char* end = (u_char*)strrchr((char*)pos, '>');
	if (end == NULL) {
		return KGL_EDATA_FORMAT;
	}
	*end = '\0';
	current_lock_token = new KWebDavLockToken;
	current_lock_token->path = path;
	current_lock_token->token = (char*)pos;

	rq->skip_body();
	return KGL_OK;
}
KGL_RESULT KWebDavClient::unlock()
{
	if (current_lock_token == nullptr) {
		return KGL_EDENIED;
	}
	KWebDavRequest* rq = NULL;
	auto result = new_request("UNLOCK", current_lock_token->path.c_str(), 0, &rq);
	if (rq == nullptr) {
		printf("webdav new_request failed error=[%d]\n", result);
		return result;
	}
	defer(delete rq);
	rq->send_lock_token(current_lock_token);
	rq->send_header_complete();
	result = rq->read_header();
	if (result != KGL_OK) {
		return result;
	}
	rq->skip_body();
	if (rq->resp.status_code == STATUS_NO_CONTENT) {
		delete current_lock_token;
		current_lock_token = nullptr;
		return KGL_OK;
	}
	return KGL_EDATA_FORMAT;	
}
KGL_RESULT KWebDavClient::copy_move(const char* method, const char* src, const char* dst, bool overwrite, KWebDavDepth depth)
{
	if (url == nullptr) {
		return KGL_ENOT_PREPARE;
	}
	KWebDavRequest* rq = NULL;
	auto result = new_request(method, src, 0, &rq);
	if (rq == nullptr) {
		return result;
	}
	defer(delete rq);
	if (current_lock_token && current_lock_token->path == dst) {
		rq->send_if_lock_token(current_lock_token, true);
	}
	KStringBuf s;
	if (!url->GetSchema(s)) {
		return KGL_ENOT_PREPARE;
	}
	url->GetHost(s);
	s << dst;
	switch (depth) {
	case KWebDavDepth::Depth_0:
		rq->send_header(kgl_expand_string("Depth"), kgl_expand_string("0"));
		break;
	case KWebDavDepth::Depth_1:
		rq->send_header(kgl_expand_string("Depth"), kgl_expand_string("1"));
		break;
	default:
		break;
	}
	
	rq->send_header(kgl_expand_string("Destination"), s.getBuf(), s.getSize());
	if (!overwrite) {
		rq->send_header(kgl_expand_string("Overwrite"), kgl_expand_string("F"));
	}
	rq->send_header_complete();
	result = rq->read_header();
	if (result != KGL_OK) {
		return result;
	}
	rq->skip_body();
	if (rq->resp.status_code == STATUS_PRECONDITION) {
		return KGL_EEXSIT;
	}
	if (rq->resp.status_code == STATUS_CREATED || rq->resp.status_code == STATUS_NO_CONTENT) {
		return KGL_OK;
	}
	return KGL_EDATA_FORMAT;
}
KGL_RESULT KWebDavClient::copy(const char* src, const char* dst,bool overwrite)
{
	return copy_move("COPY", src, dst, overwrite, KWebDavDepth::Depth_0);
}
KGL_RESULT KWebDavClient::move(const char* src, const char* dst)
{
	return copy_move("MOVE", src, dst, true, KWebDavDepth::Depth_Infinity);
}
KGL_RESULT KWebDavClient::flush_lock()
{
	if (current_lock_token == nullptr) {
		return KGL_EDENIED;
	}
	KWebDavRequest* rq = NULL;
	auto result = new_request("LOCK", current_lock_token->path.c_str(), 0, &rq);
	if (rq == nullptr) {
		printf("webdav new_request failed error=[%d]\n", result);
		return result;
	}
	defer(delete rq);
	rq->send_if_lock_token(current_lock_token);
	rq->send_header_complete();
	result = rq->read_header();
	if (result != KGL_OK) {
		return result;
	}
	rq->skip_body();
	if (rq->resp.status_code != STATUS_OK) {
		return KGL_EDATA_FORMAT;
	}
	return KGL_OK;
}
KGL_RESULT KWebDavClient::mkcol(const char* path)
{
	KWebDavRequest* rq = NULL;
	auto result = new_request("MKCOL", path, 0, &rq);
	if (rq == nullptr) {
		//printf("webdav new_request failed error=[%d]\n", result);
		return result;
	}
	defer(delete rq);
	//rq->send_if_lock_token(current_lock_token);
	rq->send_header_complete();
	result = rq->read_header();
	if (result != KGL_OK) {
		return result;
	}
	rq->skip_body();
	if (rq->resp.status_code == STATUS_CREATED) {
		return KGL_OK;
	}
	return KGL_EDATA_FORMAT;	
}
KGL_RESULT KWebDavClient::put(const char* path, KRStream* in)
{
	KWebDavRequest* rq = NULL;
	int64_t content_length = 0;
	if (in) {
		content_length = in->get_read_left();
	}
	auto result = new_request("PUT", path, content_length, &rq);
	if (rq == nullptr) {
		printf("webdav new_request failed error=[%d]\n", result);
		return result;
	}
	if (current_lock_token && current_lock_token->path == path) {
		rq->send_if_lock_token(current_lock_token);
	}
	defer(delete rq);
	rq->send_header_complete();
	char buf[1024];
	while (content_length > 0) {
		int len = in->read(buf, (int)(MIN(sizeof(buf), content_length)));
		if (len <= 0) {
			return KGL_EIO;
		}
		result = rq->write_all(buf, len);
		if (result != KGL_OK) {
			return result;
		}
		content_length -= len;
	}
	result = rq->read_header();
	if (result != KGL_OK) {
		return result;
	}
	rq->skip_body();
	if (rq->resp.status_code == STATUS_CREATED || rq->resp.status_code == STATUS_NO_CONTENT) {
		return KGL_OK;
	}
	return KGL_EDATA_FORMAT;
}
KGL_RESULT KWebDavClient::get(const char* path, KRequestRange* range, KWebDavRequest** rq)
{
	auto result = new_request("GET", path, 0, rq);
	if (rq == nullptr) {
		printf("webdav new_request failed error=[%d]\n", result);
		return result;
	}
	if (range) {
		KStringBuf s;
		s << "bytes=" << range->from << "-" << range->to;
		(*rq)->send_header(kgl_expand_string("Range"), s.getBuf(), s.getSize());
		if (!range->if_range.empty()) {
			(*rq)->send_header(kgl_expand_string("If-Range"), range->if_range.c_str(), (hlen_t)range->if_range.size());
		}
	}
	(*rq)->send_header_complete();
	return (*rq)->read_header();
}
KGL_RESULT KWebDavClient::list(const char* path, KWebDavFileList& file_list)
{
#define PROB_FIND_BODY "<?xml version=\"1.0\" encoding=\"utf-8\"?>\
<D:propfind xmlns:D=\"DAV:\">\
	<D:allprop/>\
</D:propfind>"

	KWebDavRequest* rq = NULL;
	auto result = new_request("PROPFIND", path, sizeof(PROB_FIND_BODY) - 1, &rq);
	if (rq == nullptr) {
		printf("webdav new_request failed error=[%d]\n", result);
		return result;
	}
	defer(delete rq);
	rq->send_header(kgl_expand_string("Depth"), kgl_expand_string("1"));
	rq->send_header(kgl_expand_string("Content-Type"), kgl_expand_string("application/xml; charset=\"utf-8\""));
	rq->send_header_complete();
	rq->write_all(kgl_expand_string(PROB_FIND_BODY));
	result = rq->read_header();
	if (result != KGL_OK) {
		return result;
	}
	KXmlDocument body;
	rq->read_body(body);
	//printf("status_code=[%d]\n", rq->resp.status_code);	
	if (rq->resp.status_code == STATUS_MULTI_STATUS) {
		if (!file_list.parse(body, (int)strlen(path))) {
			return KGL_EDATA_FORMAT;
		}
		return KGL_OK;
	}
	return KGL_EIO;	
}
