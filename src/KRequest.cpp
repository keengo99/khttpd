#include "KRequest.h"
#include "kselector.h"
#include "KSink.h"
#include "klog.h"
#include "KHttpLib.h"
#include "KHttpFieldValue.h"

volatile uint64_t kgl_total_requests = 0;
volatile uint64_t kgl_total_accepts = 0;
volatile uint64_t kgl_total_servers = 0;
volatile uint32_t kgl_reading = 0;
volatile uint32_t kgl_writing = 0;
volatile uint32_t kgl_waiting = 0;

KRequestData::~KRequestData()
{
	clean();
	free_lazy_memory();
	if (opaque) {
		opaque->release();
	}
}
void KRequestData::start_parse()
{
	free_lazy_memory();
	meth = METH_UNSET;
	mark = 0;
	assert(raw_url == NULL);
	assert(url == NULL);
	raw_url = new KUrl;
}
void KRequestData::free_lazy_memory()
{
	if (client_ip) {
		xfree(client_ip);
		client_ip = NULL;
	}
	if (raw_url) {
		raw_url->relase();
		raw_url = NULL;
	}
	free_header_list(header);
	header = last = NULL;
	mark = 0;
}
void KRequestData::clean()
{
	if (url) {
		url->relase();
		url = NULL;
	}
	while (fh) {
		KFlowInfoHelper* fh_next = fh->next;
		delete fh;
		fh = fh_next;
	}
}
void KRequestData::init()
{
	KRequestPlainData* data = static_cast<KRequestPlainData*>(this);
	memset(data, 0, sizeof(KRequestPlainData));
	begin_time_msec = kgl_current_msec;
}


bool KRequestData::parse_method(const char* src) {
	meth = KHttpKeyValue::getMethod(src);
	return meth >= 0;
}
bool KRequestData::parse_connect_url(char* src) {
	char* ss;
	ss = strchr(src, ':');
	if (!ss) {
		return false;
	}
	KBIT_CLR(raw_url->flags, KGL_URL_ORIG_SSL);
	*ss = 0;
	raw_url->host = strdup(src);
	raw_url->port = atoi(ss + 1);
	return true;
}
kgl_header_result KRequestData::parse_host(char* val)
{
	if (raw_url->host == NULL) {
		char* p = NULL;
		if (*val == '[') {
			KBIT_SET(raw_url->flags, KGL_URL_IPV6);
			val++;
			raw_url->host = strdup(val);
			p = strchr(raw_url->host, ']');
			if (p) {
				*p = '\0';
				p = strchr(p + 1, ':');
			}
		} else {
			raw_url->host = strdup(val);
			p = strchr(raw_url->host, ':');
			if (p) {
				*p = '\0';
			}
		}
		if (p) {
			raw_url->port = atoi(p + 1);
		} else {
			if (KBIT_TEST(raw_url->flags, KGL_URL_SSL)) {
				raw_url->port = 443;
			} else {
				raw_url->port = 80;
			}
		}
	}
	return kgl_header_no_insert;
}
bool KRequestData::parse_http_version(char* ver) {
	char* dot = strchr(ver, '.');
	if (dot == NULL) {
		return false;
	}
	http_major = *(dot - 1) - 0x30;//major;
	http_minor = *(dot + 1) - 0x30;//minor;
	return true;
}
