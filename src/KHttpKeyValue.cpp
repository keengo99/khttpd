/*
 * KHttpKeyValue.cpp
 *
 *  Created on: 2010-4-23
 *      Author: keengo
 */
#include <stdlib.h>
#include <string.h>
#include "KHttpKeyValue.h"
#include "kforwin32.h"
#include "kstring.h"
#include "kmalloc.h"
#include "KHttpLib.h"
static kgl_str_t http_methods[MAX_METHOD] = {
	kgl_string("UNSET"),
		kgl_string("GET"),
		kgl_string("HEAD"),
		kgl_string("POST"),
		kgl_string("OPTIONS"),
		kgl_string("PUT"),
		kgl_string("DELETE"),
		kgl_string("TRACE"),
		kgl_string("PROPFIND"),
		kgl_string("PROPPATCH"),
		kgl_string("MKCOL"),
		kgl_string("COPY"),
		kgl_string("MOVE"),
		kgl_string("LOCK"),
		kgl_string("UNLOCK"),
		kgl_string("ACL"),
		kgl_string("REPORT"),
		kgl_string("VERSION_CONTROL"),
		kgl_string("CHECKIN"),
		kgl_string("CHECKOUT"),
		kgl_string("UNCHECKOUT"),
		kgl_string("SEARCH"),
		kgl_string("MKWORKSPACE"),
		kgl_string("UPDATE"),
		kgl_string("LABEL"),
		kgl_string("MERGE"),
		kgl_string("BASELINE_CONTROL"),
		kgl_string("MKACTIVITY"),
		kgl_string("CONNECT"),
		kgl_string("PURGE"),
		kgl_string("PATCH"),
		kgl_string("SUBSCRIBE"),
		kgl_string("UNSUBSCRIBE")
};
kgl_str_t *KHttpKeyValue::get_method(int meth) {
	if (meth < 0 || meth >= MAX_METHOD) {
		meth = 0;
	}
	return &http_methods[meth];
}
int KHttpKeyValue::get_method(const char* src, int len)
{
	for (int i = 1; i < MAX_METHOD; i++) {
		if (kgl_mem_case_same(src, len, http_methods[i].data, http_methods[i].len)) {
			return i;
		}
	}
	return 0;
}
