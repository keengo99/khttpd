#ifndef KDECHUNK_CONTEXT_H
#define KDECHUNK_CONTEXT_H
#include "kselector.h"
#include "KDechunkEngine.h"
#include "KHttpHeaderManager.h"

class KDechunkContext : public KDechunkEngine
{
public:
	KDechunkContext(const char* buf, int length) {
		assert(length >= 0);
		ks_buffer_init(&buffer, KGL_MAX(length, 4096));
		if (length > 0) {
			memcpy(buffer.buf, buf, length);
		}
		buffer.used = length;
		hot = buffer.buf;
	}
	~KDechunkContext() {
		if (trailer) {
			free_header_list(trailer->header);
			delete trailer;
		}
		ks_buffer_clean(&buffer);
	}
	void swap(ks_buffer* buffer) {
		ks_save_point(&this->buffer, hot);
		ks_buffer tmp = this->buffer;
		this->buffer = *buffer;
		*buffer = tmp;
	}
	int read(kconnection *cn, char* buf, int length);
	friend class KHttpSink;
private:
	bool read_from_net(kconnection* cn);
	KHttpHeaderManager* get_trailer() {
		if (trailer) {
			return trailer;
		}
		trailer = new KHttpHeaderManager;
		memset(trailer, 0, sizeof(KHttpHeaderManager));
		return trailer;
	}
	void save_point() {
		ks_save_point(&buffer, hot);
		hot = buffer.buf;
	}
	ks_buffer buffer;
	const char* hot;
	KHttpHeaderManager* trailer = nullptr;
};
#endif
