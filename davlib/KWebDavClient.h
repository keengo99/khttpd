#ifndef KHTTPD_KWEBDAVCLIENT_H_99
#define KHTTPD_KWEBDAVCLIENT_H_99
#include "KUrl.h"
#include "KSockPoolHelper.h"
#include "KWebDavRequest.h"
#include "KWebDavFileList.h"
#include "KStream.h"
class KWebDavLockToken
{
public:
	std::string path;
	std::string token;
};
struct KRequestRange
{
	int64_t from;
	int64_t to;
	std::string if_range;
};
struct KWebDavAuth
{
	std::string user;
	std::string passwd;
};
enum class KWebDavDepth
{
	Depth_0,
	Depth_1,
	Depth_Infinity
};
class KWebDavClient
{
public:
	KWebDavClient();
	bool set_url(const char* url, const char* host=nullptr);
	void set_auth(const char* user, const char* passwd);
	~KWebDavClient();
	KGL_RESULT new_request(const char* method, const char* path, int64_t content_length, KWebDavRequest **rq);
	KGL_RESULT option(const char* path);
	KGL_RESULT lock(const char* path,const char *owner=nullptr);
	KGL_RESULT unlock();
	KGL_RESULT flush_lock();
	KGL_RESULT move(const char* src,const char *dst);
	KGL_RESULT copy(const char* src, const char*dst,bool overwrite=false);
	KGL_RESULT mkcol(const char* path);
	KGL_RESULT put(const char* path, KRStream* in);
	KGL_RESULT _delete(const char* path);
	KGL_RESULT get(const char* path, KRequestRange *range, KWebDavRequest **rq);
	KGL_RESULT list(const char* path, KWebDavFileList &file_list);
	friend class KWebDavRequest;
private:
	KGL_RESULT copy_move(const char *method, const char* src, const char* dst, bool overwrite, KWebDavDepth depth);
	KUrl *url;
	KWebDavAuth* auth;
	KWebDavLockToken* current_lock_token;
	KSockPoolHelper* sock_pool;
};
#endif
