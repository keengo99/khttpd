/*
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
#ifndef KXmlEvent_h_1l2kj312312
#define KXmlEvent_h_1l2kj312312
#include <map>
#include "KXmlContext.h"
#include "KStringBuf.h"
#define CDATA_START	"<![CDATA["
#define CDATA_END	"]]>"
/*
 * xml�����¼�������
 */
class KXmlEvent {
public:
	virtual ~KXmlEvent() {
	}
	virtual void startXml(const KString &encoding) {
	}
	virtual void endXml(bool success) {
	}

	/*
	 * ��ʼһ����ǩ
	 * context ��ǩ������
	 * attribute ��ǩ����
	 */
	virtual bool startElement(KXmlContext* context) {
		return false;
	}
	/*
	 * ��ʼһ����ǩ�ı�
	 * context ��ǩ������
	 * character �ı�
	 * len �ı�����
	 */
	virtual bool startCharacter(KXmlContext *context, char *character, int len) {
		return false;
	}

	/*
	 * ����һ����ǩ
	 * context ��ǩ������
	 */
	virtual bool endElement(KXmlContext *context) {
		return false;
	}

};
#endif
