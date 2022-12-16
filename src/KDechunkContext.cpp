#include "KDechunkContext.h"
#include "KHttpSink.h"
#include "kfiber.h"
bool KDechunkContext::ReadDataFromNet(KHttpSink* sink)
{
	int len;
	char* hot_buf = ks_get_read_buffer(&sink->buffer, &len);
	int got = kfiber_net_read(sink->cn, hot_buf, len);
	if (got <= 0) {
		return false;
	}
	ks_write_success(&sink->buffer, got);
	return true;
}
int KDechunkContext::Read(KHttpSink* sink, char* buf, int length)
{
	const char* piece;
	const char* hot;
	int hot_len;
	for (;;) {
		hot = sink->buffer.buf;
		hot_len = sink->buffer.used;
		if (hot_len == 0) {
			if (!ReadDataFromNet(sink)) {
				return -1;
			}
			continue;
		}		
		KDechunkResult status = dechunk(&hot, hot_len, &piece, length);
		switch (status) {
		case KDechunkResult::End:
		{
			ks_save_point(&sink->buffer, hot, hot_len);
			return 0;
		}
		case KDechunkResult::Success:
		{
			assert(piece && length > 0);
			if (buf) {
				memcpy(buf, piece, length);
			}
			ks_save_point(&sink->buffer, hot, hot_len);
			return length;
		}
		case KDechunkResult::Continue:
		{
			assert(hot_len == 0);
			ks_save_point(&sink->buffer, hot, hot_len);
			break;
		}
		default:
			return -1;
		}		
	}
}