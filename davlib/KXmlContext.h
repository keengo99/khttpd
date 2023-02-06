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
#ifndef KXMLCONTEXT_H_
#define KXMLCONTEXT_H_
#include<string>
#include<map>
#include "kstring.h"
#include "KMap.h"
#include "KHttpLib.h"
class KXmlEvent;
class KXml;
#if 0
class KXmlKeyValue
{
public:
	KXmlKeyValue(const char* name, size_t name_len, const char* val, size_t val_len) {
		key = (kgl_len_str_t*)malloc(name_len + sizeof(kgl_len_str_t) + 1);
		memcpy(key->data, name, name_len);
		key->data[name_len] = '\0';
		key->len = name_len;
		value = kstring_from2(val, val_len);
	}
	~KXmlKeyValue() {
		free(key);
		kstring_release(value);
	}
	kgl_len_str_t* key;
	kgl_ref_str_t* value;
	int cmp(kgl_str_t* a) {
		return kgl_cmp(a->data, a->len, key->data, key->len);
	}
};
class KXmlAttribute
{
public:
	KXmlAttribute() {

	}
	~KXmlAttribute() {
		m.iterator([](void* data, void* arg) {
			delete (KXmlKeyValue*)data;
			return iterator_remove_continue;
			}, NULL);
	}
	bool is_same(KXmlAttribute* a) {
		for (auto it = m.first(), it2 = a->m.first();; it = it->next(), it2 = it2->next()) {
			if (!it) {
				return !it2;
			}
			if (!it2) {
				return false;
			}
			auto n1 = it->value();
			auto n2 = it2->value();
			if (kgl_cmp(n1->key->data, n1->key->len, n2->key->data, n2->key->len) != 0) {
				return false;
			}
			if (kgl_cmp(n1->value->data, n1->value->len, n2->value->data, n2->value->len) != 0) {
				return false;
			}
		}
		return true;
	}
	kgl_refs_string* find(const char* name, size_t len) {
		kgl_str_t key;
		key.data = (char*)name;
		key.len = (int)len;
		auto it = m.find(&key);
		if (!it) {
			return nullptr;
		}
		return it->value()->value;
	}
	std::string operator[](const char* key) {
		auto v = find(key, strlen(key));
		if (v) {
			return v->data;
		}
		return "";
	}
	std::string operator[](std::string &&key) {
		auto v = find(key.c_str(), key.size());
		if (v) {
			return v->data;
		}
		return "";
	}
	KMap<kgl_str_t, KXmlKeyValue> m;
};
typedef std::map<std::string, std::string> KXmlAttribute;
#endif
class KXmlAttribute : public std::map<std::string, std::string>
{
public:
	int get_int(const char* str, size_t len) {
		auto it = find(str);
		if (it == end()) {
			return 0;
		}
		return atoi((*it).second.c_str());
	}
};
class KXmlContext
{
public:
	KXmlContext(KXml *xml);
	virtual ~KXmlContext();
	std::string getParentName();
	KXmlEvent *node;
	KXmlContext *parent;
	std::string qName;
	KXmlAttribute attribute;
	std::string path;
	int level;
	std::string getPoint();
	void *getData();
private:
	KXml *xml;
};

#endif /*KXMLCONTEXT_H_*/
