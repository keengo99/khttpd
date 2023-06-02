#include "KHttpHeader.h"
#include "KHttp2.h"

kgl_header_string kgl_header_type_string[] = {
	{_KS("Host"),_KS("host"),_KS("\r\nHost: ")},
	{_KS("Accept-Encoding"),_KS("accept-encoding"),_KS("\r\nAccept-Encoding: ")},
	{_KS("Range"),_KS("range"),_KS("\r\nRange: ")},
	{_KS("Server"),_KS("server"),_KS("\r\nServer: ")},
	{_KS("Date"),_KS("date"),_KS("\r\nDate: ")},
	{_KS("Content-Length"),_KS("content-length"),_KS("\r\nContent-Length: ")},
	{_KS("Last-Modified"),_KS("last-modified"),_KS("\r\nLast-Modified: ")},
	{_KS("Etag"),_KS("etag"),_KS("\r\nEtag: ")},
	{_KS("Content-Range"),_KS("content-range"),_KS("\r\nContent-Range: ")},
	{_KS("Content-Type"),_KS("content-type"),_KS("\r\nContent-Type: ")},
	{_KS("Set-Cookie"),_KS("set-cookie"),_KS("\r\nSet-Cookie: ")},
	{_KS("Pragma"),_KS("pragma"),_KS("\r\nPragma: ")},
	{_KS("Cache-Control"),_KS("cache-control"),_KS("\r\nCache-Control: ")},
	{_KS("Vary"),_KS("vary"),_KS("\r\nVary: ")},
	{_KS("Age"),_KS("age"),_KS("\r\nAge: ")},
	{_KS("Transfer-Encoding"),_KS("transfer-encoding"),_KS("\r\nTransfer-Encoding: ")},
	{_KS("Content-Encoding"),_KS("content-encoding"),_KS("\r\nContent-Encoding: ")},
	{_KS("Expires"),_KS("expires"),_KS("\r\nExpires: ")},
	{_KS("Location"),_KS("location"),_KS("\r\nLocation: ")},
	{_KS("Keep-Alive"),_KS("keep-alive"),_KS("\r\nKeep-Alive: ")},
	{_KS("Alt-Svc"),_KS("alt-svc"),_KS("\r\nAlt-Svc: ")},
	{_KS("Connection"),_KS("connection"),_KS("\r\nConnection: ")},
	{_KS("Upgrade"),_KS("upgrade"),_KS("\r\nUpgrade: ")},
	{_KS("Expect"),_KS("expect"),_KS("\r\nExpect: ")},
	{_KS("Status"),_KS("status"),_KS("\r\nStatus: ")},
	{_KS("If-Range"),_KS("if-range"),_KS("\r\nIf-Range: ")},
	{_KS("If-Modified-Since"),_KS("if-modified-since"),_KS("\r\nIf-Modified-Since: ")},
	{_KS("If-None-Match"),_KS("if-none-match"),_KS("\r\nIf-None-Match: ")},
	{_KS("If-Match"),_KS("if-match"),_KS("\r\nIf-Match: ")},
	{_KS("If-Unmodified-Since"),_KS("if-unmodified-since"),_KS("\r\nIf-Unmodified-Since: ")},
	{_KS("Cookie"),_KS("cookie"),_KS("\r\nCookie: ")},
	{_KS(":scheme"),_KS(":scheme"),_KS("\r\n:scheme: ")},
	{_KS("Unknow") ,_KS("unknow"),_KS("\r\nUnknow: ")},
};

#define KGL_HEADER_STRING_COUNT                                      \
    (sizeof(kgl_header_type_string)                                         \
     / sizeof(kgl_header_string))

void kgl_init_header_string() {
#ifdef ENABLE_HTTP2
	for (int i = 0; i < KGL_HEADER_STRING_COUNT; ++i) {
		kgl_header_type_string[i].http2_index = kgl_find_http2_static_table(&kgl_header_type_string[i].low_case);
		//printf("[%s] index=[%d]\n", kgl_header_type_string[i].low_case.data, kgl_header_type_string[i].http2_index);
	}
#endif
}