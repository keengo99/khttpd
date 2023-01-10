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
	int cmp(KXmlKey* a) {
		int ret = kgl_string_cmp(tag, a->tag);
		if (ret < 0) {
			return -1;
		} else if (ret > 0) {
			return 1;
		}
		return kgl_string_cmp(vary, a->vary);
	}
	kgl_ref_str_t* tag;
	kgl_ref_str_t* vary;
};
class KXmlNode
{
public:
	KXmlNode(KXmlNode* parent) {
		this->parent = parent;
		next = NULL;
		ref = 1;
	}
	int cmp(KXmlKey* a) {
		return key.cmp(a);
	}
	KXmlNode* find_child(KXmlKey* a) {
		auto it = childs.find(&key);
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
	KXmlNode* add_ref() {
		katom_inc((void*)&ref);
		return this;
	}
	void release() {
		if (katom_dec((void*)&ref) == 0) {
			delete this;
		}
	}
	bool is_same(KXmlNode* node) {
		if (kgl_string_cmp(character, node->character) != 0) {
			return false;
		}
		return attributes == node->attributes;
	}
	void addChild(KXmlNode* node) {
		//printf("add child[%s] to [%s]\n",node->getTag().c_str(),getTag().c_str());
		assert(node->parent == this);
		int new_flag;
		auto it = childs.insert(&node->key, &new_flag);
		if (new_flag) {
			it->value(node);
		} else {
			node->next = it->value();
			it->value()->next = node;
		}
	}

	std::string getTag() {
		return key.tag->data;
	}

	std::string getCharacter() {
		return character ? character->data : "";
	}
	KXmlKey key;
	KMap<KXmlKey, KXmlNode> childs;
	KXmlAttribute attributes;
	KXmlNode* next;
	KXmlNode* parent;
	kgl_refs_string* character = nullptr;
private:
	volatile uint32_t ref;
	~KXmlNode() {
		childs.iterator([](void* data, void* arg) {
			KXmlNode* node = (KXmlNode*)data;
			while (node) {
				node->parent = NULL;
				node = node->next;
			}
			((KXmlNode*)data)->release();
			return iterator_remove_continue;
			}, NULL);
#if 0
		for (auto it = childs.begin(); it != childs.end(); it++) {
			auto child = (*it).second;
			while (child) {
				assert(child->parent == this);
				child->parent = NULL;
				child = child->next;
			}
			(*it).second->release();
		}
#endif
		if (next) {
			next->release();
		}
		assert(parent == NULL);
		kstring_release(character);
	}
};
class KXmlDocument :
	public KXmlEvent
{
public:
	KXmlDocument(bool skip_ns = true);
	~KXmlDocument(void);
	void set_vary(std::map<std::string, std::string>* vary) {
		this->vary = vary;
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
	std::map<std::string, std::string>* vary = nullptr;
};
#endif
