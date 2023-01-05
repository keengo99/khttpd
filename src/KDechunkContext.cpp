#include "KDechunkContext.h"
#include "KHttpSink.h"
#include "kfiber.h"
bool KDechunkContext::read_from_net(KHttpSink* sink) {
	int len;
	char* hot_buf = ks_get_read_buffer(&sink->buffer, &len);
	int got = kfiber_net_read(sink->cn, hot_buf, len);
	if (got <= 0) {
		return false;
	}
	ks_write_success(&sink->buffer, got);
	return true;
}
int KDechunkContext::read(KHttpSink* sink, char* buf, int length) {
	const char* piece;
	const char* hot;
	for (;;) {
		hot = sink->buffer.buf;
		const char* end = hot + sink->buffer.used;
		if (hot == end) {
			if (!read_from_net(sink)) {
				return -1;
			}
			continue;
		}

	continue_dechunk:
		int piece_length = length;
		KDechunkResult status = dechunk(&hot, end, &piece, &piece_length);
		switch (status) {
		case KDechunkResult::Trailer:
		{
			const char* trailer_end = piece + piece_length;
			const char* sp = (char*)memchr(piece, ':', piece_length);
			if (sp == nullptr) {
				goto continue_dechunk;
			}
			hlen_t attr_len = (hlen_t)(sp - piece);
			sp++;
			while (sp < trailer_end && isspace((unsigned char)*sp)) {
				sp++;
			}
			get_trailer()->add_header(piece, attr_len, sp, (hlen_t)(trailer_end - sp));
			goto continue_dechunk;
		}
		case KDechunkResult::End:
		{
			ks_save_point(&sink->buffer, hot);
			return 0;
		}
		case KDechunkResult::Success:
		{
			assert(piece && piece_length > 0);
			if (buf) {
				memcpy(buf, piece, piece_length);
			}
			ks_save_point(&sink->buffer, hot);
			return piece_length;
		}
		case KDechunkResult::Continue:
		{
			ks_save_point(&sink->buffer, hot);
			if (!read_from_net(sink)) {
				return -1;
			}
			break;
		}
		default:
			return -1;
		}
	}
}