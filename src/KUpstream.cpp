#include "KUpstream.h"
#include "KPoolableSocketContainer.h"
#include "klog.h"
#include "KRequest.h"
#include "kselector.h"
struct kgl_upstream_delay_io
{
	KUpstream *us;
	void *arg;
	buffer_callback buffer;
	result_callback result;
};
KUpstream::~KUpstream()
{
	if (container) {
		container->unbind(this);
	}
}
void KUpstream::IsBad(BadStage stage)
{
	if (container) {
		container->isBad(this, stage);
	}
}
void KUpstream::IsGood()
{
	if (container) {
		container->isGood(this);
	}
}
int KUpstream::GetLifeTime()
{
	if (container) {
		return container->getLifeTime();
	}
	return 0;
}
