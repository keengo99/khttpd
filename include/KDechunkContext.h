#ifndef KDECHUNK_CONTEXT_H
#define KDECHUNK_CONTEXT_H
#include "kselector.h"
#include "KDechunkEngine.h"
#include "KHttpHeaderManager.h"

class KHttpSink;
class KDechunkContext : public KDechunkEngine
{
public:
	~KDechunkContext() {
		if (trailer) {
			free_header_list(trailer->header);
			delete trailer;
		}
	}
	int read(KHttpSink* sink, char* buf, int length);
	friend class KHttpSink;
private:
	bool read_from_net(KHttpSink* sink);
	KHttpHeaderManager* get_trailer() {
		if (trailer) {
			return trailer;
		}
		trailer = new KHttpHeaderManager;
		memset(trailer, 0, sizeof(KHttpHeaderManager));
		return trailer;
	}
	KHttpHeaderManager* trailer = nullptr;
};
#endif
