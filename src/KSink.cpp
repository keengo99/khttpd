#include "KSink.h"
#include "klog.h"
#include "KHttpFieldValue.h"
#include "kfiber.h"
#include "kselector_manager.h"
#include "kfiber_sync.h"
#ifdef KGL_DEBUG_TIME
struct kgl_timespec {
	int64_t total;
	int count;
};
struct kgl_timespec total_spec = { 0 };
#define NS_PER_SECOND 1000000000
void sub_timespec(struct timespec t1, struct timespec t2, struct timespec* td) {
	td->tv_nsec = t2.tv_nsec - t1.tv_nsec;
	td->tv_sec = t2.tv_sec - t1.tv_sec;
	if (td->tv_sec > 0 && td->tv_nsec < 0) {
		td->tv_nsec += NS_PER_SECOND;
		td->tv_sec--;
	} else if (td->tv_sec < 0 && td->tv_nsec>0) {
		td->tv_nsec -= NS_PER_SECOND;
		td->tv_sec++;
	}
}
void KSink::log_passed_time(const char* tip) {
	struct timespec end, delta;
	clock_gettime(CLOCK_REALTIME, &end);
	sub_timespec(start_time, end, &delta);
	klog(KLOG_ERR, "%p %s passed_time=[%d.%09ld]\n", this, tip, (int)delta.tv_sec, delta.tv_nsec);
	if (*tip == '+') {
		int64_t v = delta.tv_sec * NS_PER_SECOND + delta.tv_nsec;
		total_spec.total += v;
		total_spec.count++;
	}
	reset_start_time();
	return;
}
void kgl_log_total_timespec() {
	kgl_timespec a = total_spec;
	if (a.count > 0) {
		klog(KLOG_ERR, "count=[%d] avg time=[%lld]\n", a.count, a.total / a.count);
	}
}
#endif

pthread_key_t kgl_request_key;
kev_result kgl_request_thread_init(KOPAQUE data, void* arg, int got) {
	if (got == 0) {
		//init
		kgl_request_ts* rq = new kgl_request_ts;
		klist_init(&rq->sinks);
		pthread_setspecific(kgl_request_key, rq);
	} else {
		//shutdown
		kgl_request_ts* rq = (kgl_request_ts*)pthread_getspecific(kgl_request_key);
		klist_empty(&rq->sinks);
		delete rq;
		pthread_setspecific(kgl_request_key, NULL);
	}
	return kev_ok;
}
struct kgl_sink_iterator_param
{
	void* ctx;
	kgl_sink_iterator it;
	kfiber_cond* cond;
};
static kev_result ksink_iterator(KOPAQUE data, void* arg, int got) {
	kgl_sink_iterator_param* param = (kgl_sink_iterator_param*)arg;
	kgl_request_ts* ts = (kgl_request_ts*)pthread_getspecific(kgl_request_key);
	kgl_list* pos;
	klist_foreach(pos, &ts->sinks) {
		KSink* sink = (KSink*)kgl_list_data(pos, KSink, queue);
		if (!param->it(param->ctx, sink)) {
			break;
		}
	}
	param->cond->f->notice(param->cond, got);
	return kev_ok;
}
void kgl_iterator_sink(kgl_sink_iterator it, void* ctx) {
	kgl_sink_iterator_param param;
	param.ctx = ctx;
	param.it = it;
	param.cond = kfiber_cond_init_ts(true);
	auto selector_count = get_selector_count();
	for (int i = 0; i < selector_count; i++) {
		kselector* selector = get_selector_by_index(i);
		kgl_selector_module.next(selector, NULL, ksink_iterator, &param, 0);
		param.cond->f->wait(param.cond, NULL);
	}
	param.cond->f->release(param.cond);
}

bool KSink::response_100_continue() {
	if (!internal_response_status(100)) {
		return false;
	}
	if (internal_start_response_body(0, true) < 0) {
		return false;
	}
	flush();
	return true;
}
const char* KSink::get_state() {
	switch (data.state) {
	case STATE_IDLE:
		return "idle";
	case STATE_SEND:
		return "send";
	case STATE_RECV:
		return "recv";
	case STATE_WAIT:
		return "wait";
	}
	return "unknow";
}

int KSink::read(char* buf, int len) {
	KBIT_SET(data.flags, RQ_HAS_READ_POST);
	kassert(!kfiber_is_main());
	if (KBIT_TEST(data.flags, RQ_HAVE_EXPECT)) {
		KBIT_CLR(data.flags, RQ_HAVE_EXPECT);
		response_100_continue();
	}
	int length;
	if (data.left_read >= 0 && !KBIT_TEST(data.flags, RQ_CONNECTION_UPGRADE)) {
		len = (int)KGL_MIN((int64_t)len, data.left_read);
	}
	length = internal_read(buf, len);
	if (length == 0 && data.left_read == -1 && !KBIT_TEST(data.flags, RQ_CONNECTION_UPGRADE)) {
		data.left_read = 0;
		return 0;
	}
	if (length > 0) {
		if (data.left_read > 0) {
			assert(data.left_read >= length);
			data.left_read -= length;
		}
		add_up_flow(length);
	}
	return length;
}
bool kgl_init_sink_queue() {
	pthread_key_create(&kgl_request_key, NULL);
	if (0 != selector_manager_thread_init(kgl_request_thread_init, NULL)) {
		klog(KLOG_ERR, "init_http_server_callback must called early!!!\n");
		return false;
	}
	return true;
}
