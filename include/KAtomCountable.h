#ifndef KCOUNTABLE_H_99sss
#define KCOUNTABLE_H_99sss
#include "katom.h"

/* ref for base class */
#define KGL_REF_CLASS(CLASSNAME, VIRTUAL) \
	uint32_t get_ref() { return katom_get((void *)&ref_count);}\
	CLASSNAME *add_ref() { katom_inc((void*)&ref_count); return this;}; \
	void release() {if (katom_dec((void*)&ref_count)==0) delete this;};\
protected: \
	volatile uint32_t ref_count = 1; \
	VIRTUAL ~CLASSNAME() noexcept
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
