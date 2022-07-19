#ifndef KDECHUNKENGINE_H
#define KDECHUNKENGINE_H
#include "khttp.h"
#include <stdlib.h>
#include <string.h>
#include "kmalloc.h"
#include "kbuf.h"
#define KHTTPD_MAX_CHUNK_SIZE 0x10000000
enum class KDechunkResult
{
	Success,//����ɹ��������ݷ��أ���Ҫ��������
	Continue,//����ɹ��������ݷ��أ�Ҫ����ι����
	End,//����ɹ��������ݷ��أ��������
	Failed//�������
};
enum dechunk_status
{
	dechunk_success, //����ɹ��������ݷ��أ���Ҫ��������
	dechunk_continue,//����ɹ��������ݷ��أ�Ҫ����ι����
	dechunk_end,     //����ɹ��������ݷ��أ��������
	dechunk_failed   //�������
};

class KDechunkEngine2
{
public:
	KDechunkEngine2()
	{
		chunk_size = status_read_chunk_size;
	}
	//piece_length��in,out������inʱָʾ���鳤��
	KDechunkResult dechunk(char ** buf, int& buf_len, char** piece, int& piece_length);
	uint32_t chunk_size;
private:
	static constexpr uint32_t status_read_chunk_size{ static_cast<uint32_t>(0) };
	static constexpr uint32_t status_read_end{ static_cast<uint32_t>(-1) };
	static constexpr uint32_t status_read_last{ static_cast<uint32_t>(-2) };
	static constexpr uint32_t status_is_ended{ static_cast<uint32_t>(-3) };
	static constexpr uint32_t status_is_failed{ static_cast<uint32_t>(-4) };
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
	bool is_success() {
		return work_len == -4;
	}
	//����dechunk_continue��ʾ����Ҫ�����ݣ�
	dechunk_status dechunk(const char **buf, int&buf_len, const char **piece, int &piece_length,int max_piece_length=0);
private:
	bool is_failed() {
		return work_len < -4;
	}
	bool is_end() {
		return work_len <= -4;
	}
	int chunk_size ;
	int work_len ;
	char *work;
};

template<typename T>
class KDechunkReader
{
public:
	KDechunkReader(T* us)
	{
		this->us = us;
		hot = buffer;
		hot_len = 0;
	}
	~KDechunkReader()
	{

	}
	int read(char* buf, int len)
	{
		char* piece;
		for (;;) {
			switch (engine.dechunk(&hot, hot_len, &piece, len)) {
			case KDechunkResult::Success:
				assert(piece && len > 0);
				kgl_memcpy(buf, piece, len);
				return len;
			case KDechunkResult::Continue:
			{
				kgl_memcpy(buffer, hot, hot_len);
				hot = buffer;
				int got = us->read(buffer + hot_len, sizeof(buffer) - hot_len);
				if (got <= 0) {
					return -1;
				}
				hot_len += got;
				//fwrite(buffer, 1, hot_len, stdout);
				continue;
			}
			case KDechunkResult::End:
			{
				return 0;
			}
			default:
				return -1;
			}
		}
	}
private:
	T* us;
	KDechunkEngine2 engine;
	char buffer[8192];
	int hot_len;
	char* hot;
};
#endif
