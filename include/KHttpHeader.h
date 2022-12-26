#ifndef KHTTPHEADER_H_
#define KHTTPHEADER_H_
#include "kmalloc.h"
#include "kstring.h"
#include "khttp.h"


#define KGL_HEADER_VALUE_INT64 1
#define KGL_HEADER_VALUE_INT   2
#define KGL_HEADER_VALUE_TIME  3
char *make_http_time(time_t time, char* buf, int size);
KBEGIN_DECLS
typedef struct _kgl_header_string
{
	kgl_str_t value;
	kgl_str_t low_case;
	kgl_str_t http11;
} kgl_header_string;

extern kgl_header_string kgl_header_type_string[];
#define kgl_value_type(type) (type<<16)
#define kgl_cpymem(dst, src, n)   (((u_char *) kgl_memcpy(dst, src, n)) + (n))
inline void kgl_get_header_name(KHttpHeader* header, kgl_str_t* result)
{
	if (header->name_is_know) {
		result->data = kgl_header_type_string[header->know_header].value.data;
		result->len = kgl_header_type_string[header->know_header].value.len;
		return;
	}
	result->data = header->buf;
	result->len = header->name_len;
	return;
}
inline KHttpHeader* new_pool_http_header(kgl_pool_t* pool, const char* attr, int attr_len, const char* val, int val_len) {
	if (attr_len > MAX_HEADER_ATTR_VAL_SIZE || val_len > MAX_HEADER_ATTR_VAL_SIZE) {
		return NULL;
	}
	KHttpHeader* header = (KHttpHeader*)kgl_pnalloc(pool, sizeof(KHttpHeader));
	memset(header, 0, sizeof(KHttpHeader));
	header->buf = (char*)kgl_pnalloc(pool, attr_len + val_len + 2);
	kgl_memcpy(header->buf, attr, attr_len);
	header->buf[attr_len] = '\0';
	header->val_offset = attr_len + 1;
	kgl_memcpy(header->buf + header->val_offset, val, val_len);
	header->buf[attr_len + val_len + 1] = '\0';
	header->val_len = val_len;
	header->name_len = attr_len;
	return header;
}
inline KHttpHeader* new_http_know_header(kgl_header_type type, const char* val, int val_len) {
	assert(type>=0 && type<kgl_header_unknow);
	KHttpHeader* header = (KHttpHeader*)malloc(sizeof(KHttpHeader));
	memset(header, 0, sizeof(KHttpHeader));
	int val_type = (val_len >> 16);
	switch (val_type) {
	case KGL_HEADER_VALUE_INT64:
		header->buf = (char*)malloc(KGL_INT64_LEN+1);
		header->val_len = snprintf(header->buf, KGL_INT64_LEN, INT64_FORMAT, *(int64_t*)val);
		break;
	case KGL_HEADER_VALUE_TIME:
	{
		header->buf = (char*)malloc(KGL_1123_TIME_LEN+1);
		char* end = make_http_time(*(time_t*)val, header->buf, KGL_1123_TIME_LEN);
		header->val_len = (hlen_t)(end - header->buf);
		break;
	}
	default:
		header->buf = (char*)malloc(val_len+1);
		kgl_memcpy(header->buf, val, val_len);
		header->val_len = val_len;
		break;
	}
	header->buf[header->val_len] = '\0';
	header->know_header = (uint16_t)type;
	header->name_is_know = 1;
	return header;
}
inline KHttpHeader* new_http_header2(const char* attr, int attr_len, const char* val, int val_len) {
	if (attr_len > MAX_HEADER_ATTR_VAL_SIZE || val_len > MAX_HEADER_ATTR_VAL_SIZE) {
		return NULL;
	}
	KHttpHeader* header = (KHttpHeader*)malloc(sizeof(KHttpHeader));
	memset(header, 0, sizeof(KHttpHeader));
	header->buf = (char*)malloc(attr_len + val_len + 2);
	kgl_memcpy(header->buf, attr, attr_len);
	header->buf[attr_len] = '\0';
	header->val_offset = attr_len + 1;
	kgl_memcpy(header->buf + header->val_offset, val, val_len);
	header->buf[attr_len + val_len + 1] = '\0';
	header->val_len = val_len;
	header->name_len = attr_len;
	return header;
}
inline void xfree_header2(KHttpHeader* av) {
	free(av->buf);
	free(av);
}
inline void free_header_list2(KHttpHeader* av) {
	KHttpHeader* next;
	while (av) {
		next = av->next;
		xfree_header2(av);
		av = next;
	}
}
KEND_DECLS
#endif /*KHTTPHEADER_H_*/
