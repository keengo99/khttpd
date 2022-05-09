#ifndef KDECHUNKENGINE_H
#define KDECHUNKENGINE_H
#include "khttp.h"
#include <stdlib.h>
#include <string.h>
#include "kmalloc.h"
#include "kbuf.h"
#define KHTTPD_MAX_CHUNK_SIZE 0x100000000
enum class KDechunkResult
{
	Success,//解码成功，有数据返回，但要继续解码
	Continue,//解码成功，有数据返回，要继续喂数据
	End,//解码成功，无数据返回，解码结束
	Failed//解码错误
};
enum dechunk_status
{
	dechunk_success, //解码成功，有数据返回，但要继续解码
	dechunk_continue,//解码成功，有数据返回，要继续喂数据
	dechunk_end,     //解码成功，无数据返回，解码结束
	dechunk_failed   //解码错误
};

class KDechunkEngine2
{
public:
	KDechunkEngine2()
	{
		chunk_size = status_read_chunk_size;
	}
	//piece_length是in,out参数，in时指示最大块长度
	KDechunkResult dechunk(char ** buf, int& buf_len, char** piece, int& piece_length);
	uint32_t chunk_size;
private:
	static constexpr auto status_read_chunk_size{ static_cast<uint32_t>(0) };
	static constexpr auto status_read_end{ static_cast<uint32_t>(-1) };
	static constexpr auto status_read_last{ static_cast<uint32_t>(-2) };
	static constexpr auto status_is_ended{ static_cast<uint32_t>(-3) };
	static constexpr auto status_is_failed{ static_cast<uint32_t>(-4) };
};
class KDechunkEngine {
public:
	KDechunkEngine() {
		chunk_size = 0;
		work_len = 0;
		work = NULL;
	}
	~KDechunkEngine() {
		if (work) {
			free(work);
		}
	}
	//返回dechunk_continue表示还需要读数据，
	dechunk_status dechunk(const char **buf, int&buf_len, const char **piece, int &piece_length,int max_piece_length=0);
	dechunk_status dechunk(ks_buffer *buf, char* out,int& out_len);
private:
	bool is_failed() {
		return work_len < -4;
	}
	bool is_success() {
		return work_len == -4;
	}
	bool is_end() {
		return work_len <= -4;
	}
	int chunk_size ;
	int work_len ;
	char *work;
};
#endif
