#include "KRequest.h"
#include "kselector.h"
#include "KSink.h"
#include "klog.h"
#include "KHttpLib.h"
#include "KHttpFieldValue.h"

volatile uint64_t kgl_total_requests = 0;
volatile uint64_t kgl_total_accepts = 0;
volatile uint64_t kgl_total_servers = 0;
volatile uint32_t kgl_reading = 0;
volatile uint32_t kgl_writing = 0;
volatile uint32_t kgl_waiting = 0;

