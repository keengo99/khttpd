#ifndef KURL_H
#define KURL_H
#ifdef _WIN32
#pragma warning(disable:4003)
#include <direct.h>
#endif
#include "khttp.h"
#include "KStringBuf.h"
#include "kforwin32.h"
#include "kmalloc.h"
#include "KHttpLib.h"
#include "katom.h"


#define KGL_URL_SSL       1   //根据端口判断是否是ssl
#define KGL_URL_IPV6      2
//#define KGL_URL_VARIED    4
#define KGL_URL_REWRITED  8
#define KGL_URL_RANGED    0x10
#define KGL_URL_ENCODE    0x20
#define KGL_URL_ORIG_SSL  0x40  //源建议的ssl
//#define KGL_URL_BAD       0x80

#define KGL_ENCODING_DEFLATE  1
#define KGL_ENCODING_COMPRESS (1<<1)
#define KGL_ENCODING_GZIP     (1<<2)
#define KGL_ENCODING_BR       (1<<3)
#define KGL_ENCODING_UNKNOW   (1<<6)
//#define KGL_ENCODING_YES      (1<<7)

class KUrl {

public:
	KUrl() {
		memset(this, 0, sizeof(KUrl));
		refs_count = 1;
	}
	

	bool match_accept_encoding(u_char accept_encoding) {
		if (encoding > 0) {
			return KBIT_TEST(accept_encoding,encoding) > 0  && KBIT_TEST(this->accept_encoding,accept_encoding) == accept_encoding;
		}
		return KBIT_TEST(this->accept_encoding, accept_encoding) == accept_encoding;
	}
	void set_content_encoding(u_char content_encoding) {
		this->encoding = content_encoding;
	}
	void merge_accept_encoding(u_char accept_encoding) {
		if (encoding>0) {
			return;
		}
		KBIT_SET(this->accept_encoding, accept_encoding);
	}
	int cmpn(const KUrl *a,int n) const {
		int ret = strcasecmp(host,a->host);
		if (ret<0) {
			return -1;
		} else if (ret > 0) {
			return 1;
		}
		return strncmp(path,a->path,n);
	}
	int cmp(const KUrl *a) const {
		int ret = strcasecmp(host, a->host);
		if (ret < 0) {
			return -1;
		} else if (ret > 0) {
			return 1;
		}
		ret = strcmp(path, a->path);
		if (ret < 0) {
			return -1;
		} else if (ret > 0) {
			return 1;
		}
		if (port < a->port) {
			return -1;
		} else if(port > a->port) {
			return 1;
		}
		if (param == NULL) {
			if (a->param == NULL) {
				return 0;
			} else {
				return 1;
			}
		}
		if (a->param == NULL) {
			return -1;
		}
		return strcmp(param, a->param);
	}
	KUrl* refs() {
		katom_inc16((void*)&refs_count);
		return this;
	}
	bool IsBad() {
		return host == NULL || path == NULL;
	}
	char *getUrl() {
		KStringBuf s(128);
		if (!GetUrl(s)) {
			return NULL;
		}
		return s.stealString();
	}
	char *getUrl2(int &len) {
		KStringBuf s(128);
		if (!GetUrl(s)) {
			return NULL;
		}
		len = s.getSize();
		return s.stealString();
	}
	void GetPath(KStringBuf &s, bool urlEncode = false) {
		if (urlEncode) {
			size_t len = strlen(path);
			char *newPath = url_encode(path, len, &len);
			if (newPath) {
				s.write_all(newPath, (int)len);
				free(newPath);
			}
		} else {
			s << path;
		}
		if (param && *param) {
			if (urlEncode) {
				size_t len = strlen(param);
				char *newParam = url_value_encode(param, len, &len);
				if (newParam) {
					s.write_all("?", 1);
					s.write_all(newParam, (int)len);
					free(newParam);
				}
			}
			else {
				s << "?" << param;
			}
		}
	}
	void GetHost(KStringBuf &s, uint16_t default_port)
	{
		if (unlikely(KBIT_TEST(flags, KGL_URL_IPV6))) {
			s << "[" << host << "]";
		} else {
			s << host;
		}
		if (unlikely(port != default_port)) {
			s << ":" << port;
		}
	}
	bool GetSchema(KStringBuf& s)
	{
		if (unlikely(host == NULL || path == NULL)) {
			return false;
		}
		if (KBIT_TEST(flags, KGL_URL_SSL)) {
			s << "https://";
		} else {
			s << "http://";
		}
		return true;
	}
	bool GetUrl(KStringBuf &s,bool urlEncode=false) {
		if (!GetSchema(s)) {
			return false;
		}
		GetHost(s);
		GetPath(s, urlEncode);
		return true;
	}
	void GetHost(KStringBuf& s)
	{
		int default_port = 80;
		if (KBIT_TEST(flags, KGL_URL_SSL)) {
			default_port = 443;
		}
		GetHost(s, default_port);
	}
	void relase()
	{
		if (katom_dec16((void*)&refs_count) > 0) {
			return;
		}
		delete this;
	}
	bool parse_host(const char* val, size_t len)
	{
		assert(this->host == NULL);
		char* port;
		if (*val == '[') {
			KBIT_SET(this->flags, KGL_URL_IPV6);
			val++;
			len--;
			char* host_end = (char*)memchr(val, ']', len);
			if (host_end == NULL) {
				return false;
			}
			size_t host_len = host_end - val;
			port = (char*)memchr(host_end, ':', len - host_len);
			if (port) {
				size_t port_len = len - host_len;
				port_len -= (port - host_end);
				this->port = (uint16_t)kgl_atoi((u_char*)port + 1, port_len - 1);
			}
			len = host_len;
		} else {
			port = (char*)memchr(val, ':', len);
			if (port) {
				size_t port_len = len;
				len = port - val;
				port_len -= len;
				this->port = (uint16_t)kgl_atoi((u_char*)port + 1, port_len - 1);
			}
		}
		this->host = kgl_strndup(val, len);
		if (port == NULL) {
			if (KBIT_TEST(this->flags, KGL_URL_ORIG_SSL)) {
				this->port = 443;
			} else {
				this->port = 80;
			}
		}
		return true;
	}
	char *host;
	char *path;
	char *param;
	volatile uint16_t refs_count;
	uint16_t port;
	union {
		uint32_t flag_encoding;
		struct {
			u_char flags;
			u_char accept_encoding;
			u_char encoding;
			u_char reserv;
		};
	};
private:

	~KUrl() {
		IF_FREE(host);
		IF_FREE(path);
		IF_FREE(param);
#ifndef NDEBUG
		flag_encoding = 0;
#endif
	}
};
class KAutoUrl
{
public:
	KAutoUrl()
	{
		u = new KUrl;		
	}
	~KAutoUrl()
	{
		u->relase();
	}
	KUrl* u;
};
#endif
