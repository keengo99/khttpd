#ifndef KHTTPD_KWEBDAVREQUEST_H
#define KHTTPD_KWEBDAVREQUEST_H
#include "KUpstream.h"
#include "KHttpHeaderManager.h"
#include "KXmlDocument.h"
#include "kbuf.h"
#include "KDechunkEngine.h"

class KWebDavLockToken;
struct KWebDavAuth;
class KWebDavClient;
bool webdav_header_callback(KUpstream* us, void* arg, const char* attr, int attr_len, const char* val, int val_len, bool is_first);
class KResponseData : public KHttpHeaderManager
{
public:
	uint16_t status_code;
	int64_t left;
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
		us->set_no_delay(false);
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
		if (resp.left == 0) {
			return 0;
		}
		if (resp.left > 0) {
			assert(dechunk == nullptr);
			len = (int)(MIN(resp.left, len));
			int got = us->read(buf, len);
			if (got <= 0) {
				return -1;
			}
			resp.left -= got;
			return got;
		}
		if (dechunk) {
			int got = dechunk->read(buf, len);
			if (got == 0) {
				resp.left = 0;
			}
			return got;
		}
		return us->read(buf, len);
	}
	int read_all(char* buf, int len) 
	{
		int total_len = 0;
		while (len > 0) {
			int got = read(buf, len);
			if (got == 0) {
				return total_len;
			}
			if (got < 0) {
				return -1;
			}
			buf += got;
			len -= got;
			total_len += got;
		}
		return total_len;
	}
	int64_t get_left()
	{
		return resp.left;
	}
	KGL_RESULT skip_body();
	ks_buffer *read_body(KGL_RESULT &result);
	KGL_RESULT read_body(KXmlDocument &body);
	KResponseData resp;
	KDechunkReader<KUpstream>* dechunk;
private:
	KUpstream* us;
	KWebDavClient* client;

};
#endif

