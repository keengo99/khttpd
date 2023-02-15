#ifndef KXMLDOCUMENT_H
#define KXMLDOCUMENT_H
#include <assert.h>
#include <list>
#include <stack>
#include "KXmlEvent.h"
#include "KAtomCountable.h"
#include "KMap.h"
#include "kstring.h"
#include "KHttpLib.h"
#include "KStream.h"
#include "klog.h"

class KXmlKey
{
public:
	KXmlKey(const char* tag, size_t size) {
		const char* p = (char*)memchr(tag, '@', size);
		if (!p) {
			this->tag = kstring_from2(tag, size);
			this->vary = nullptr;
			return;
		}
		this->tag = kstring_from2(tag, p - tag);
		this->vary = kstring_from2(p + 1, size - this->tag->len - 1);
	}
	KXmlKey(kgl_refs_string* tag, kgl_refs_string* vary) {
		this->tag = kstring_refs(tag);
		this->vary = kstring_refs(vary);
	}
	KXmlKey() {
		tag = nullptr;
		vary = nullptr;
	}
	void set_tag(const std::string& tag) {
		assert(this->tag == nullptr);
		this->tag = kstring_from2(tag.c_str(), tag.size());
	}
	~KXmlKey() {
		kstring_release(tag);
		kstring_release(vary);
	}
	bool is_wide() {
		return *(tag->data) == '*';
	}
	int cmp(kgl_ref_str_t* key) {
		return kgl_cmp(tag->data, tag->len, key->data, key->len);
	}
	int cmp(KXmlKey* a) {
		int ret = cmp(a->tag);
		if (ret != 0) {
			return ret;
		}
		return kgl_string_cmp(vary, a->vary);
	}
	kgl_ref_str_t* tag;
	kgl_ref_str_t* vary;
};
class KXmlNode
{
public:
	KXmlNode() {
		next = NULL;
		ref = 1;
	}
	KXmlNode(KXmlKey* name) :key(name->tag, name->vary) {
		next = NULL;
		ref = 1;
	}
	int cmp(KXmlKey* a) {
		int result = (int)a->tag->id - (int)key.tag->id;
		if (result != 0) {
			return result;
		}
		if (key.tag->id != 0) {
			//know id
			return result;
		}
		return key.cmp(a);
	}
	bool is_tag(const char* tag, size_t len) {
		return kgl_cmp(key.tag->data, key.tag->len, tag, len) == 0;
	}
	KXmlNode* find_child(KXmlKey* a) {
		auto it = childs.find(a);
		if (!it) {
			return nullptr;
		}
		return it->value();
	}
	KXmlNode* getChild(std::string tag) {
		size_t pos = tag.find('/');
		std::string childtag;
		if (pos != std::string::npos) {
			childtag = tag.substr(0, pos);
		} else {
			childtag = tag;
		}
		KXmlKey key(childtag.c_str(), childtag.size());
		auto it = childs.find(&key);
		if (!it) {
			return nullptr;
		}
		if (pos != std::string::npos) {
			return it->value()->getChild(tag.substr(pos + 1));
		}
		return it->value();
	}
	KXmlNode* getNext() {
		return next;
	}
	KXmlNode* clone() {
		KXmlNode* node = new KXmlNode(&key);
		node->attributes = attributes;
		node->character = kstring_refs(character);
		if (next) {
			node->next = next->clone();
		}
		for (auto it = childs.first(); it; it = it->next()) {
			node->append(it->value()->clone());
		}
		return node;
	}

	KXmlNode* add_ref() {
		katom_inc((void*)&ref);
		return this;
	}
	void release() {
		if (katom_dec((void*)&ref) == 0) {
			delete this;
		}
	}
	KGL_RESULT write(KWStream* out) {
		out->write_all(_KS("<"));
		out->write_all(key.tag->data, key.tag->len);
		for (auto it = attributes.begin(); it != attributes.end(); it++) {
			out->write_all(_KS(" "));
			out->write_all((*it).first.c_str(), (int)(*it).first.size());
			out->write_all(_KS("="));

			if ((*it).second.find('\'') == std::string::npos) {
				out->write_all(_KS("'"));
				out->write_all((*it).second.c_str(), (int)(*it).second.size());
				out->write_all(_KS("'"));
			} else if ((*it).second.find('"') == std::string::npos) {
				out->write_all(_KS("\""));
				out->write_all((*it).second.c_str(), (int)(*it).second.size());
				out->write_all(_KS("\""));
			} else {
				klog(KLOG_ERR, "cann't write xml attribute [%s %s] value also has ['\"]\n", key.tag->data, (*it).first.c_str());
				out->write_all(_KS("''"));
			}
		}
		//write attribute
		out->write_all(_KS(">"));
		//write child
		for (auto it = childs.first(); it; it = it->next()) {
			out->write_all(_KS("\n"));
			auto result = it->value()->write(out);
			if (result != KGL_OK) {
				return result;
			}
		}
		if (character) {
			if (memchr(character->data, '<', character->len)) {
				out->write_all(_KS(CDATA_START));
				out->write_all(character->data, character->len);
				out->write_all(_KS(CDATA_END));
			} else {
				out->write_all(character->data, character->len);
			}
		}
		out->write_all(_KS("</"));
		out->write_all(key.tag->data, key.tag->len);
		auto result = out->write_all(_KS(">\n"));
		if (result != KGL_OK) {
			return result;
		}
		if (next) {
			return next->write(out);
		}
		return result;
	}
	bool is_same(KXmlNode* node) {
		if (kgl_string_cmp(character, node->character) != 0) {
			return false;
		}
		return attributes == node->attributes;
	}
	bool update(KXmlKey* key, int index, KXmlNode* xml) {
		auto it = childs.find(key);
		if (!it) {
			return false;
		}
		KXmlNode* last = nullptr;
		while (index > 0) {
			index--;
			if (last) {
				last = last->next;
			} else {
				last = it->value();
			}
			if (!last) {
				return false;
			}
		}
		if (!last) {
			auto node = it->value();
			if (xml) {
				it->value(xml->add_ref());
			} else {
				if (node->next) {
					it->value(node->next);
				} else {
					childs.erase(it);
				}
			}
			node->release();
			return true;
		}
		if (!last->next) {
			return false;
		}
		auto node = last->next;
		if (xml) {
			xml->next = node->next;
			last->next = xml->add_ref();
		} else {
			last->next = node->next;
		}
		node->release();
		return true;
	}
	void append(KXmlNode* node) {
		int new_flag;
		auto it = childs.insert(&node->key, &new_flag);
		if (new_flag) {
			it->value(node);
			return;
		}
		auto last = it->value();
		while (last->next) {
			last = last->next;
		}
		last->next = node;
	}
	/* insert before index node */
	void insert(KXmlNode* node, int index) {
		if (index == -1) {
			return append(node);
		}
		int new_flag;
		auto it = childs.insert(&node->key, &new_flag);
		if (new_flag) {
			it->value(node);
			return;
		}
		KXmlNode* last = nullptr;
		while (index > 0) {
			index--;
			if (last) {
				if (!last->next) {
					break;
				}
				last = last->next;
			} else {
				last = it->value();
			}
		}
		if (last) {
			node->next = last->next;
			last->next = node;
		} else {
			node->next = it->value();
			it->value(node);
		}

	}

	std::string getTag() {
		return key.tag->data;
	}
	const char* get_text() {
		return character ? character->data : "";
	}
	std::string getCharacter() {
		return character ? character->data : "";
	}
	KXmlKey key;
	KMap<KXmlKey, KXmlNode> childs;
	KXmlAttribute attributes;
	KXmlNode* next;
	kgl_ref_str_t* character = nullptr;
private:
	volatile uint32_t ref;
	~KXmlNode() {
		childs.iterator([](void* data, void* arg) {
			KXmlNode* node = (KXmlNode*)data;
			((KXmlNode*)data)->release();
			return iterator_remove_continue;
			}, NULL);
		if (next) {
			next->release();
		}
		kstring_release(character);
	}
};
class KXmlDocument :
	public KXmlEvent
{
public:
	KXmlDocument(bool skip_ns = true);
	~KXmlDocument(void);
	void set_qname_config(KMap<kgl_ref_str_t, KXmlKey>* qname_config) {
		this->qname_config = qname_config;
	}
	KXmlNode* parse(char* str);
	KXmlNode* getRootNode();
	KXmlNode* getNode(std::string name);
	bool startElement(KXmlContext* context) override;
	bool startCharacter(KXmlContext* context, char* character, int len) override;
	bool endElement(KXmlContext* context) override;
private:
	bool skip_ns;
	KXmlNode* cur_node = nullptr;
	KXmlNode* root = nullptr;
	KXmlNode* cur_child_brother = nullptr;
	std::stack<KXmlNode*> brothers;
	std::stack<KXmlNode*> parents;
	KMap<kgl_ref_str_t, KXmlKey>* qname_config = nullptr;
};
#endif
