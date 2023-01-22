/*
 * KStream.cpp
 *
 *  Created on: 2010-5-10
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
#include <string>
#include <sstream>
#include <stdlib.h>
#include <stdio.h>

#include "kforwin32.h"
#include "KStream.h"
#include "kmalloc.h"
KConsole KConsole::out;
bool KRStream::read_all(char *buf, int len) {
	while (len > 0) {
		int r = read(buf, len);
		if (r <= 0)
			return false;
		len -= r;
		buf += r;
	}
	return true;
}
KGL_RESULT KWStream::write_all(WSABUF* bufs, int bc) {
	for (int i = 0; i < bc; i++) {
		KGL_RESULT result = write_all((char *)bufs[i].iov_base, bufs[i].iov_len);
		if (result != KGL_OK) {
			return result;
		}
	}
	return KGL_OK;
}
KGL_RESULT KWStream::write_all(const char *buf, int len) {
	while (len > 0) {
		int r = write(buf, len);
		if (r <= 0) {
			return KGL_EIO;
		}
		len -= r;
		buf += r;
	}
	return KGL_OK;
}
KGL_RESULT KWStream::write_all(const char *buf) {
	return write_all(buf, (int)strlen(buf));
}
int KConsole::write(const char *buf, int len) {
	return (int)fwrite(buf, 1, len, stdout);
}
