#ifndef KHTTPSTREAM_H
#define KHTTPSTREAM_H
#include "KStream.h"
#include "kmalloc.h"
#include "kfiber.h"
#include "ksapi.h"
#include "klist.h"

class KHttpStream : public KWStream
{
public:
	KHttpStream(KWStream* st) {
		this->st = st;
	}
	virtual ~KHttpStream() {
		if (st) {
			st->release();
		}
	}
	virtual KGL_RESULT flush() override {
		if (st) {
			return st->flush();
		}
		return STREAM_WRITE_FAILED;
	}
	virtual KGL_RESULT write_all(const char* buf, int len) override {
		if (st) {
			return st->write_all( buf, len);
		}
		return STREAM_WRITE_FAILED;
	}
	virtual KGL_RESULT write_end( KGL_RESULT result) override {
		if (st) {
			return st->write_end(result);
		}
		return result;
	}
protected:
	KWStream* st;
};
#endif
