/*
 * KString.cpp
 *
 *  Created on: 2010-4-27
 *      Author: keengo
 * Copyright (c) 2010, NanChang BangTeng Inc
 * All Rights Reserved.
 *
 * You may use the Software for free for non-commercial use
 * under the License Restrictions.
 *
 * You may modify the source code(if being provieded) or interface
 * of the Software under the License Restrictions.
 *
 * You may use the Software for commercial use after purchasing the
 * commercial license.Moreover, according to the license you purchased
 * you may get specified term, manner and content of technical
 * support from NanChang BangTeng Inc
 *
 * See COPYING file for detail.
 */

#include <string.h>
#include <stdio.h>
#include "KStringBuf.h"
#include "kmalloc.h"
#include "kforwin32.h"
 /*
  * return the next string
  *
  */
char* getString(char* str, char** nextstr, const char* ended_chars,
	bool end_no_quota_value, bool skip_slash) {
	while (*str && isspace((unsigned char)*str)) {
		str++;
	}
	bool slash = false;
	char endChar = *str;
	char* start;
	if (endChar != '\'' && endChar != '"') {
		//û���������������ַ���
		start = str;
		while (*str && !isspace((unsigned char)*str)) {
			if (ended_chars && strchr(ended_chars, *str) != NULL) {
				break;
			}
			str++;
		}
		if (end_no_quota_value) {
			if (*str != '\0') {
				*str = '\0';
				str++;
			}
		}
		*nextstr = str;
		return start;
	}
	str++;
	start = str;
	char* hot = str;
	while (*str) {
		if (slash) {
			if (*str == '\\' || *str == '\'' || *str == '"') {
				*hot = *str;
				hot++;
			} else {
				*hot = '\\';
				hot++;
				*hot = *str;
				hot++;
			}
			slash = false;
		} else {
			if (!skip_slash && *str == '\\') {
				slash = true;
				str++;
				continue;
			}
			slash = false;
			if (*str == endChar) {
				*str = '\0';
				*hot = '\0';
				*nextstr = str + 1;
				return start;
			}
			*hot = *str;
			hot++;
		}
		str++;
	}
	return NULL;
}
