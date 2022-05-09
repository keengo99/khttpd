#ifndef KHTTP_H_SADFLKJASDLFKJA455ss
#define KHTTP_H_SADFLKJASDLFKJA455ss
#include "kfeature.h"
#ifdef KSOCKET_SSL
	#define ENABLE_UPSTREAM_SSL   1
	#define ENABLE_HTTP2          1
	#ifdef ENABLE_HTTP2
		#define ENABLE_UPSTREAM_HTTP2 1
	#endif
#endif
#define MAX_HTTP_HEAD_SIZE	4194304
#define IF_FREE(p) {if ( p ) xfree(p);p=NULL;}


#define STATUS_OK               200
#define STATUS_CREATED          201
#define STATUS_NO_CONTENT       204
#define STATUS_CONTENT_PARTIAL  206
#define STATUS_MULTI_STATUS     207
#define STATUS_MOVED            301
#define STATUS_FOUND            302
#define STATUS_NOT_MODIFIED     304
#define STATUS_TEMPORARY_REDIRECT 307
#define STATUS_BAD_REQUEST      400
#define STATUS_UNAUTH           401
#define STATUS_FORBIDEN         403
#define STATUS_NOT_FOUND        404
#define STATUS_METH_NOT_ALLOWED 405
#define STATUS_PROXY_UNAUTH     407
#define STATUS_CONFLICT         409
#define STATUS_PRECONDITION     412
#define STATUS_LOCKED           423
#define STATUS_HTTP_TO_HTTPS    497
#define STATUS_SERVER_ERROR     500
#define STATUS_NOT_IMPLEMENT    501
#define STATUS_BAD_GATEWAY      502
#define STATUS_SERVICE_UNAVAILABLE  503
#define STATUS_GATEWAY_TIMEOUT  504



/**
* rq->flags
*/
#define RQ_QUEUED              1
#define RQ_HAS_IF_MOD_SINCE    (1<<1)
#define RQ_HAS_IF_NONE_MATCH   (1<<2)
#define RQ_HAS_NO_CACHE        (1<<3)
#define RQ_BODY_NOT_COMPLETE   (1<<4)
#define RQ_CACHE_HIT           (1<<5)
#define RQ_INPUT_CHUNKED       (1<<6)
#define RQ_SYNC                (1<<7)
#define RQ_HAS_ONLY_IF_CACHED  (1<<8)
#define RQ_HAS_AUTHORIZATION   (1<<9)
#define RQ_HAS_PROXY_AUTHORIZATION (1<<10)
#define RQ_HAS_KEEP_CONNECTION (1<<11)
#define RQ_CONNECTION_UPGRADE  (1<<12)
#define RQ_HAS_CONNECTION_UPGRADE  (1<<13)
#define RQ_IF_RANGE_DATE       (1<<14)
#define RQ_IF_RANGE_ETAG       (1<<15)
#define RQ_HAS_CONTENT_LEN     (1<<16)
#define RQ_OBJ_VERIFIED        (1<<17)
#define RQ_HAVE_RANGE          (1<<18)
#define RQ_TE_CHUNKED          (1<<19)
#define RQ_TE_COMPRESS         (1<<20)
#define RQ_HAS_SEND_HEADER     (1<<21)

#define RQ_POST_UPLOAD         (1<<23)
#define RQ_CONNECTION_CLOSE    (1<<24)
#define RQ_OBJ_STORED          (1<<25)
#define RQ_HAVE_EXPECT         (1<<26)

#define RQ_TEMPFILE_HANDLED    (1<<28)
#define RQ_IS_ERROR_PAGE       (1<<29)
#define RQ_UPSTREAM            (1<<30)
#define RQ_UPSTREAM_ERROR      (1<<31)



///////////////////////////////////////////////
//rq->filter_flag
///////////////////////////////////////////////
#define  RF_TPROXY_UPSTREAM  (1)
#ifdef ENABLE_TPROXY
#define  RF_TPROXY_TRUST_DNS (1<<1)
#endif
#define  RF_PROXY_RAW_URL    (1<<2) /*raw_url */
#define  RF_DOUBLE_CACHE_EXPIRE (1<<3)
#define  RF_UPSTREAM_NOKA    (1<<4)
#define  RF_ALWAYS_ONLINE    (1<<5)
#define  RF_GUEST            (1<<6)
#define  RQ_NO_EXTEND        (1<<7)
#define  RF_X_REAL_IP        (1<<8)
#define  RF_NO_X_FORWARDED_FOR  (1<<9)
#define  RF_LOG_DRILL        (1<<10)
#define  RF_NO_CACHE         (1<<12)
#define  RF_VIA              (1<<13)
#define  RF_X_CACHE          (1<<14)
#define  RF_NO_BUFFER        (1<<15)
#define  RF_NO_DISK_CACHE    (1<<16)
#define  RF_UPSTREAM_NOSNI   (1<<17)
#define  RQ_SEND_AUTH        (1<<18)
#define  RF_PROXY_FULL_URL   (1<<19)
#define  RF_FOLLOWLINK_ALL   (1<<21)
#define  RF_FOLLOWLINK_OWN   (1<<22)
#define  RF_NO_X_SENDFILE    (1<<23)
#define  RQ_URL_QS           (1<<25)
#define  RQ_FULL_PATH_INFO   (1<<26)
#define  RF_AGE              (1<<27)
#define  RQ_SWAP_OLD_OBJ     (1<<28)
#define  RQ_RESPONSE_DENY    (1<<29)
#define  RQF_CC_PASS         (1<<30)
#define  RQF_CC_HIT          (1<<31)

#define WORK_MODEL_MANAGE    (1<<1)
#define WORK_MODEL_SSL       (1<<2)
#define WORK_MODEL_TCP       (1<<3)
#ifdef  ENABLE_PROXY_PROTOCOL
#define WORK_MODEL_PROXY     (1<<5)
//proxy in ssl
#define WORK_MODEL_SSL_PROXY (1<<6)
#endif
#define WORK_MODEL_TPROXY    (1<<7)

#define  NBUFF_SIZE     8192

#define KGL_REQUEST_POOL_SIZE 4096
#define        IS_SPACE(a)     isspace((unsigned char)a)
#define        IS_DIGIT(a)     isdigit((unsigned char)a)
KBEGIN_DECLS
typedef unsigned short hlen_t;
typedef enum _kgl_header_type
{
	kgl_header_unknow = 0,
	kgl_header_internal,
	kgl_header_server,
	kgl_header_date,
	kgl_header_content_length,
	kgl_header_last_modified,
} kgl_header_type;

#define MAX_HEADER_ATTR_VAL_SIZE 65500
typedef struct _KHttpHeader KHttpHeader;

struct	_KHttpHeader {
	char* attr;
	char* val;
	hlen_t attr_len;
	hlen_t val_len;
	kgl_header_type type;
	KHttpHeader* next;
};

typedef enum _KGL_RESULT
{
	KGL_OK = 0,
	KGL_END = 1,
	KGL_NO_BODY = 2,
	KGL_RETRY = 3,
	KGL_DOUBLICATE = 4,
	KGL_NEXT = 5,
	KGL_EINSUFFICIENT_BUFFER = -1,
	KGL_ENO_DATA = -2,
	KGL_EINVALID_PARAMETER = -3,
	KGL_EINVALID_INDEX = -4,
	KGL_ENOT_READY = -5,
	KGL_EDATA_FORMAT = -6,
	KGL_ENO_MEMORY = -7,
	KGL_EDENIED = -8,
	KGL_EHAS_SEND_HEADER = -11,
	KGL_ESOCKET_BROKEN = -20,
	KGL_EIO = -21,
	KGL_ENOT_PREPARE = -22,
	KGL_EEXSIT = -23,
	KGL_ESYSCALL = -98,
	KGL_EUNKNOW = -99,
	KGL_ENOT_SUPPORT = -100
} KGL_RESULT;

KEND_DECLS
#endif
