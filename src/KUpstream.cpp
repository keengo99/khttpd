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
void KUpstream::health(HealthStatus stage)
{
	if (container) {
		container->health(this, stage);
	}
}
int KUpstream::GetLifeTime()
{
	if (container) {
		return container->getLifeTime();
	}
	return 0;
}
kgl_refs_string* KUpstream::get_param()
{
	if (container == NULL) {
		return NULL;
	}
	return container->GetParam();
}
