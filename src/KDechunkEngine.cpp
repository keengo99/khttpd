#include "KDechunkEngine.h"
#include "KHttpLib.h"
KDechunkResult KDechunkEngine::dechunk(const char** buf, const char* end, const char** piece, int* piece_length)
{
	int length;
restart:
	switch (chunk_size) {
	case KHTTPD_CHUNK_STATUS_IS_FAILED:
		assert(false);
		return KDechunkResult::Failed;
	case KHTTPD_CHUNK_STATUS_IS_END:
		assert(false);
		return KDechunkResult::End;
	case KHTTPD_CHUNK_STATUS_READ_LAST:
	case KHTTPD_CHUNK_STATUS_READ_END:
	{
		const char* next_line = (const char*)memchr(*buf, '\n', end - (*buf));
		if (next_line == NULL) {
			*buf = end;
			return KDechunkResult::Continue;
		}
		(*buf) += (int)(next_line - (*buf) + 1);
		if (chunk_size == KHTTPD_CHUNK_STATUS_READ_LAST) {
			chunk_size = KHTTPD_CHUNK_STATUS_IS_END;
			return KDechunkResult::End;
		}
		chunk_size = KHTTPD_CHUNK_STATUS_READ_SIZE;
		//���ﲻ��breakֱ��fallthrough,������status_read_chunk_size
	}
	default:
		assert(KBIT_TEST(chunk_size, KHTTPD_CHUNK_STATUS) != KHTTPD_CHUNK_STATUS);
		if (chunk_size == KHTTPD_CHUNK_STATUS_READ_SIZE ||
			KBIT_TEST(chunk_size, KHTTPD_CHUNK_PART_SIZE) == KHTTPD_CHUNK_PART_SIZE) {
			for (;;) {
				if (*buf>=end) {
					return KDechunkResult::Continue;
				}
				u_char ch = *((u_char*)*buf);
				if (ch == '\n') {
					KBIT_CLR(chunk_size, KHTTPD_CHUNK_STATUS_PREFIX);
					(*buf)++;					
					if (chunk_size == 0) {
						chunk_size = KHTTPD_CHUNK_STATUS_READ_LAST;
						goto restart;
					}
					break;
				}
				if (KBIT_TEST(chunk_size, KHTTPD_CHUNK_PART_SIZE_END) == KHTTPD_CHUNK_PART_SIZE_END) {
					goto next_buf;
				}
				if (ch >= '0' && ch <= '9') {
					uint32_t new_chunk_size = ((chunk_size) & ~(KHTTPD_CHUNK_STATUS_PREFIX)) * 16 + (ch - '0');
					if (new_chunk_size > KHTTPD_MAX_CHUNK_SIZE) {
						chunk_size = KHTTPD_CHUNK_STATUS_IS_FAILED;
						return KDechunkResult::Failed;
					}
					chunk_size = new_chunk_size | KHTTPD_CHUNK_PART_SIZE;
					goto next_buf;
				}
				ch = (u_char)(ch | 0x20);
				if (ch >= 'a' && ch <= 'f') {
					uint32_t new_chunk_size = ((chunk_size) & ~(KHTTPD_CHUNK_STATUS_PREFIX)) * 16 + (ch - 'a' + 10);
					if (new_chunk_size > KHTTPD_MAX_CHUNK_SIZE) {
						chunk_size = KHTTPD_CHUNK_STATUS_IS_FAILED;
						return KDechunkResult::Failed;
					}
					chunk_size = new_chunk_size | KHTTPD_CHUNK_PART_SIZE;
					goto next_buf;
				}
				KBIT_SET(chunk_size, KHTTPD_CHUNK_PART_SIZE_END);
			next_buf:
				(*buf)++;
			}
		}
		assert(*piece_length >= 0 && *piece_length <= KHTTPD_MAX_CHUNK_SIZE);
		assert(KBIT_TEST(chunk_size, KHTTPD_CHUNK_STATUS_PREFIX) == 0);
		assert(chunk_size > 0);
		length = (int)(end - *buf);
		length = KGL_MIN((int)chunk_size, length);
		if (length <= 0) {
			return KDechunkResult::Continue;
		}
		length = KGL_MIN(*piece_length, length);
		*piece = *buf;
		*piece_length = length;
		(*buf) += length;
		chunk_size -= length;
		if (chunk_size == 0) {
			chunk_size = KHTTPD_CHUNK_STATUS_READ_END;
		}
		return KDechunkResult::Success;
	}
}
KDechunkResult KDechunkEngine::dechunk(const char** buf, int buf_len, const char** piece, int* piece_length)
{
	return dechunk(buf, (*buf) + buf_len, piece, piece_length);
}
