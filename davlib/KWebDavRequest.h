#ifndef KHTTPD_KWEBDAVREQUEST_H
#define KHTTPD_KWEBDAVREQUEST_H
#include "KUpstream.h"
#include "KHttpHeaderManager.h"
#include "KXmlDocument.h"
#include "kbuf.h"

class KWebDavLockToken;
struct KWebDavAuth;
class KWebDavClient;
bool webdav_header_callback(KUpstream* us, void* arg, const char* attr, int attr_len, const char* val, int val_len, bool is_first);
class KResponseData : public KHttpHeaderManager
{
public:
	uint16_t status_code;
};
class KWebDavRequest
{
public:
	KWebDavRequest(KWebDavClient *client,KUpstream *us);
	~KWebDavRequest();
	bool send_header(const char* attr, hlen_t attr_len, const char* val, hlen_t val_len)
	{
		return us->send_header(attr, attr_len, val, val_len);
	}
	KGL_RESULT send_header_complete()
	{
		return us->send_header_complete();
	}
	KGL_RESULT read_header()
	{
		return us->read_header();
	}
	bool send_http_auth(KWebDavAuth* auth);
	bool send_if_lock_token(KWebDavLockToken* token, bool send_resource=false);
	bool send_lock_token(KWebDavLockToken* token);
	KGL_RESULT write_all(const char* buf, int len)
	{
		return us->write_all(buf, len);
	}
	int read(char* buf, int len)
	{
		return us->read(buf, len);
	}
	bool read_all(char* buf, int len) {
		while (len > 0) {
			int got = read(buf, len);
			if (got <= 0) {
				return false;
			}
			buf += got;
			len -= got;
		}
		return true;
	}
	int64_t get_left()
	{
		return us->get_left();
	}
	KGL_RESULT skip_body();
	ks_buffer *read_body(KGL_RESULT &result);
	KGL_RESULT read_body(KXmlDocument &body);
	KResponseData resp;
private:
	KUpstream* us;
	KWebDavClient* client;

};
#endif

