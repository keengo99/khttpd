#include "KDechunkContext.h"
#include "KHttpSink.h"
#include "kfiber.h"
bool KDechunkContext::read_from_net(kconnection* cn) {
	int len;
	char* hot_buf = ks_get_read_buffer(&buffer, &len);
	int got = kfiber_net_read(cn, hot_buf, len);
	if (got <= 0) {
		return false;
	}
#if 0
	printf("*[");
	fwrite(hot_buf, 1, got, stdout);
	printf("]\n");
#endif
	ks_write_success(&buffer, got);
	return true;
}
int KDechunkContext::read(kconnection* cn, char* buf, int length) {
	const char* piece;
	for (;;) {
		const char* end = buffer.buf + buffer.used;
		assert(hot >= buffer.buf && hot <= end);
		if (hot == end) {
			save_point();
			if (!read_from_net(cn)) {
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
			while (sp < trailer_end && KHTTP_ISSPACE((unsigned char)*sp)) {
				sp++;
			}
			get_trailer()->add_header(piece, attr_len, sp, (hlen_t)(trailer_end - sp));
			goto continue_dechunk;
		}
		case KDechunkResult::End:
		{
			save_point();
			return 0;
		}
		case KDechunkResult::Success:
		{
			assert(piece && piece_length > 0);
			memmove(buf, piece, piece_length);
			return piece_length;
		}
		case KDechunkResult::Continue:
		{
			save_point();
			if (!read_from_net(cn)) {
				return -1;
			}
			break;
		}
		default:
			return -1;
		}
	}
}