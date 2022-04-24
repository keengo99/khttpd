#ifndef KCOUNTABLE_H_99sss
#define KCOUNTABLE_H_99sss
#include "katom.h"

class KAtomCountable
{
public:
	KAtomCountable()
	{
		refs = 1;
	}
	int addRef()
	{
		return katom_inc((void*)&refs);
	}
	int release()
	{
		int ret = katom_dec((void*)&refs);
		if (ret == 0) {
			delete this;
		}
		return ret;
	}
	int getRef()
	{
		return katom_get((void*)&refs);
	}
protected:
	virtual ~KAtomCountable()
	{
	};
private:
	volatile int32_t refs;
};
#endif
