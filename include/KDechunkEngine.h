#ifndef KDECHUNKENGINE_H
#define KDECHUNKENGINE_H
#include "khttp.h"
#include <stdlib.h>
#include <string.h>
#include "kmalloc.h"
#include "kbuf.h"

enum class KDechunkResult
{
	Success,//解码成功，有数据返回，但要继续解码
	Continue,//解码成功，无数据返回，要继续喂数据
	End,//解码成功，无数据返回，解码结束
	Failed//解码错误
};
#define dechunk_success  KDechunkResult::Success
#define dechunk_continue KDechunkResult::Continue
#define dechunk_end      KDechunkResult::End
#define dechunk_failed   KDechunkResult::Failed
#define dechunk_status   KDechunkResult

#define KHTTPD_MAX_CHUNK_SIZE      0x1FFFFFFF    /* 000111----1 */
#define KHTTPD_CHUNK_STATUS_PREFIX 0xE0000000    /* 1110------0 */
#define KHTTPD_CHUNK_PART_SIZE_END 0xA0000000    /* 1010------0 */
#define KHTTPD_CHUNK_STATUS        0xC0000000    /* 1100------0 */
#define KHTTPD_CHUNK_PART_SIZE     0x80000000    /* 1000------0 */


#define KHTTPD_CHUNK_STATUS_READ_SIZE  0
#define KHTTPD_CHUNK_STATUS_READ_END   (KHTTPD_CHUNK_STATUS|1)
#define KHTTPD_CHUNK_STATUS_READ_LAST  (KHTTPD_CHUNK_STATUS|2)
#define KHTTPD_CHUNK_STATUS_IS_END     (KHTTPD_CHUNK_STATUS|4)
#define KHTTPD_CHUNK_STATUS_IS_FAILED  (KHTTPD_CHUNK_STATUS|8)
class KDechunkEngine
{
public:
	KDechunkEngine()
	{
		chunk_size = KHTTPD_CHUNK_STATUS_READ_SIZE;
	}
	//piece_length是in,out参数，in时指示最大块长度
	KDechunkResult dechunk(const char** buf, const char* end, const char** piece, int* piece_length);
	KDechunkResult dechunk(const char** buf, int buf_len, const char** piece, int* piece_length);
	bool is_success() {
		return chunk_size == KHTTPD_CHUNK_STATUS_IS_END;
	}
private:
	uint32_t chunk_size;
};
template<typename T>
class KDechunkReader
{
public:
	KDechunkReader(T* us)
	{
		this->us = us;
	}
	int read(char* buf, int len)
	{
		char* dst = buf;
	retry:
		char* src = dst;
		len -= (int)(dst - buf);
		if (engine.is_success()) {
			return (int)(dst - buf);
		}
		int got = us->read(src, len);
		if (got <= 0) {
			return -1;
		}
		const char* piece;
		const char* end = src + got;
		for (;;) {
			int piece_length = KHTTPD_MAX_CHUNK_SIZE;
			switch (engine.dechunk((const char**)&src, end, &piece, &piece_length)) {
			case KDechunkResult::Success:
				kgl_memcpy(dst, piece, piece_length);
				dst += piece_length;
				break;
			case KDechunkResult::Continue:
				goto retry;
			case KDechunkResult::End:
				return (int)(dst - buf);
			default:
				return -1;
			}
		}
	}
private:
	T* us;
	KDechunkEngine engine;
};
#endif
