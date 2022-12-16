#ifndef KDECHUNK_CONTEXT_H
#define KDECHUNK_CONTEXT_H
#include "kselector.h"
#include "KDechunkEngine.h"
class KHttpSink;
class KDechunkContext : public KDechunkEngine
{
public:
	int Read(KHttpSink* sink, char* buf, int length);
private:
	bool ReadDataFromNet(KHttpSink* sink);
};
#endif
