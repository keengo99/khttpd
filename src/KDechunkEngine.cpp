#include "KDechunkEngine.h"


KDechunkResult KDechunkEngine2::dechunk(char** buf, int& buf_len, char** piece, int& piece_length)
{
	char* next_line;
	int length;
restart:
	switch (chunk_size) {
	case status_is_failed:
		assert(false);
		return KDechunkResult::Failed;
	case status_is_ended:
		assert(false);
		return KDechunkResult::End;
	case status_read_last:
	case status_read_end:
	{
		next_line = (char*)memchr(*buf, '\n', buf_len);
		if (next_line == NULL) {
			return KDechunkResult::Continue;
		}
		length = (int)(next_line - (*buf) + 1);
		(*buf) += length;
		buf_len -= length;
		if (chunk_size == status_read_last) {
			chunk_size = status_is_ended;
			return KDechunkResult::End;
		}
		chunk_size = status_read_chunk_size;
		//���ﲻ��breakֱ��fallthrough,������status_read_chunk_size
	}
	case status_read_chunk_size:
	{
		next_line = (char*)memchr(*buf, '\n', buf_len);
		if (next_line == NULL) {
			return KDechunkResult::Continue;
		}
		chunk_size = strtol(*buf, NULL, 16);
		if (chunk_size > KHTTPD_MAX_CHUNK_SIZE) {
			chunk_size = status_is_failed;
			return KDechunkResult::Failed;
		}
		length = (int)(next_line - (*buf) + 1);
		(*buf) += length;
		buf_len -= length;
		if (chunk_size == 0) {
			chunk_size = status_read_last;
			goto restart;
		}
		//���ﲻ��breakֱ��fallthrough,������default����
	}
	default:
		assert((int)chunk_size > 0);
		length = MIN((int)chunk_size, buf_len);
		if (length <= 0) {
			return KDechunkResult::Continue;
		}
		length = MIN(piece_length, length);
		*piece = *buf;
		piece_length = length;
		buf_len -= length;
		(*buf) += length;
		chunk_size -= length;
		if (chunk_size == 0) {
			chunk_size = status_read_end;
		}
		return KDechunkResult::Success;
	}
}
dechunk_status KDechunkEngine::dechunk(const char **buf, int &buf_len, const char **piece, int &piece_length,int max_piece_length)
{
	char *next_line;
	char *data;
	int data_len;
	*piece = NULL;
	if (is_success()) {
		return dechunk_end;
	}
	if (is_failed()) {
		return dechunk_failed;
	}
restart:
	//��body��������\n
	if (work_len == -1 || work_len == -3) {
		next_line = (char *)memchr(*buf, '\n', buf_len);
		if (next_line == NULL) {
			return dechunk_continue;//continue;
		}
		int skip_length = (int)(next_line - (*buf) + 1);
		(*buf) += skip_length;
		buf_len -= skip_length;
		//	chunk_size = 0;
		//��ȷ����
		if (work_len == -3) {
			//assert(buf_len==0);
			//skip double 0\r\n0\r\n this will cause buf_len>0
			assert(work == NULL);
			work_len = -4;
			//	printf("success dechunk buf\n");
			return dechunk_end;
		}
		work_len = 0;
		if (buf_len == 0) {
			//	printf("�պù�����,����\n");
			return dechunk_continue;
		}
	}
	//��ʾ��chunk_size
	if (work_len >= 0) {
		if (work) {
			int read_len = MIN(20 - work_len, buf_len);
			//assert(read_len>0);
			if (read_len <= 0) {
				//	printf("chunk size ̫����,20λ������,����!");
				free(work);
				work = NULL;
				work_len = -5;
				return dechunk_failed;
			}
			kgl_memcpy(work + work_len, *buf, read_len);
			work_len += read_len;
			data = work;
			data_len = work_len;
		} else {
			data = (char *)(*buf);
			data_len = buf_len;
		}
		next_line = (char *)memchr(data, '\n', data_len);
		if (next_line == NULL) {//����Ŷ
			if (work) {
				//continue;
				return dechunk_continue;
			}
			work = (char *)malloc(buf_len + 20);
			work_len = buf_len;
			kgl_memcpy(work, *buf, buf_len);
			//continue;
			return dechunk_continue;
		}
		chunk_size = strtol(data, NULL, 16);
		work_len = -2;
		//	printf("chunk_size=%d\n",chunk_size);
		if (chunk_size == 0 && data[0] != '0') {
			//	printf("read chunk size failed %d,data[0]=%c\n", chunk_size,
			//			data[0]);
			//assert(false);
			if (work) {
				free(work);
				work = NULL;
			}
			work_len = -5;
			return dechunk_failed;
		}
		if (work) {
			free(work);
			work = NULL;
		}
		if (chunk_size < 0 || chunk_size > KHTTPD_MAX_CHUNK_SIZE) {
			//assert(false);
			//printf("chunk size ����ȷ,%d\n",chunk_size);
			work_len = -5;
			return dechunk_failed;
		}
		//����
		next_line = (char *)memchr(*buf, '\n', buf_len);
		assert(next_line);
		int skip_length = (int)(next_line - (*buf) + 1);
		(*buf) += skip_length;
		buf_len -= skip_length;
		if (chunk_size == 0) {
			work_len = -3;
			goto restart;
		}
	}
	//��body
	if (work_len == -2) {
		int read_len = MIN(chunk_size, buf_len);
		if (read_len > 0) {
			bool continue_read_piece = false;
			if (max_piece_length > 0) {
				if (read_len > max_piece_length) {
					read_len = max_piece_length;
					continue_read_piece = true;
				}
			}
			*piece = *buf;
			piece_length = read_len;
			buf_len -= read_len;
			(*buf) += read_len;
			chunk_size -= read_len;
			if (continue_read_piece) {
				return dechunk_success;
			}
		}
		if (buf_len > 0) {
			kassert(chunk_size >= 0);
			work_len = -1;
			//	chunk_size = -1;
			//	printf("ţ!����chunk�ɶ�.\n");
			//goto restart;
			return dechunk_success;
		}
		if (chunk_size == 0) {
			//	chunk_size = -1;
			work_len = -1;
		}
		//printf("chunk ���ݲ���,����һ��buf\n");
		return dechunk_continue;
	}
	//printf("�����ܵ�������Ŷ!��bug\n");
	work_len = -5;
	assert(false);
	return dechunk_failed;
}