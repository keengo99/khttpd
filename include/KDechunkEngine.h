#ifndef KDECHUNKENGINE_H
#define KDECHUNKENGINE_H
#include "khttp.h"
#include <stdlib.h>
#include <string.h>
#include "kmalloc.h"
#include "kbuf.h"

enum dechunk_status
{
	dechunk_success, //����ɹ��������ݷ��أ���Ҫ��������
	dechunk_continue,//����ɹ��������ݷ��أ�Ҫ����ι����
	dechunk_end,     //����ɹ��������ݷ��أ��������
	dechunk_failed   //�������
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
	//����dechunk_continue��ʾ����Ҫ�����ݣ�
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
