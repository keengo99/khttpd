/*
 * KHttpFieldValue.h
 *
 *  Created on: 2010-6-3
 *      Author: keengo
 */

#ifndef KHTTPFIELDVALUE_H_
#define KHTTPFIELDVALUE_H_
#include "KHttpLib.h"

class KHttpFieldValue
{
public:
	KHttpFieldValue(const char* val, const char* end)
	{
		this->val = val;
		this->end = end;
	}
	bool contain(const char* field, size_t len)
	{
		do {
			if (is(field, len)) {
				return true;
			}
		} while (next());
		return false;
	}
	bool is(const char* field, size_t field_len)
	{
		if ((size_t)(end - val) < field_len) {
			return false;
		}
		return strncasecmp(val, field, field_len) == 0;
	}
	bool is(const char* field, size_t field_len, int* n)
	{
		if ((size_t)(end - val) < field_len) {
			return false;
		}
		if (strncasecmp(val, field, field_len) != 0) {
			return false;
		}
		if (val + field_len >= end) {
			return false;
		}
		val += field_len;
		*n = kgl_atoi((u_char*)val, end - val);
		return true;

	}
	bool next()
	{
		val = (const char*)memchr(val, ',', end - val);
		if (val == NULL) {
			return false;
		}
		val++;
		while (val < end && isspace((unsigned char)*val)) {
			val++;
		}
		if (val == end) {
			return false;
		}
		return true;

	}
private:
	const char* val;
	const char* end;
};
#endif /* KHTTPFIELDVALUE_H_ */
