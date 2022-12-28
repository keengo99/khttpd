#ifndef KDECHUNK_CONTEXT_H
#define KDECHUNK_CONTEXT_H
#include "kselector.h"
#include "KDechunkEngine.h"
class KHttpSink;
class KDechunkContext : public KDechunkEngine
{
public:
	int read(KHttpSink* sink, char* buf, int length);
private:
	bool read_from_net(KHttpSink* sink);
};
#endif
