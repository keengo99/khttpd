#include "KHttp2Sink.h"
#ifdef ENABLE_HTTP2
bool KHttp2Sink::parse_header(const char* attr, int attr_len, const char* val, int val_len) {
	if (ctx->parsed_header) {
		//it is trailer
		return ctx->get_trailer_header()->add_header(attr, attr_len, val, val_len) != nullptr;
	}
	return KSink::parse_header<const char *>(attr, attr_len, val, val_len, false);
}
#endif
