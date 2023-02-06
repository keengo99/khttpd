#include <stdlib.h>
#include <string.h>
#include <vector>
#include "KXml.h"
#include "KXmlDocument.h"
using namespace std;
KXmlDocument::KXmlDocument(bool skip_ns) {
	this->skip_ns = skip_ns;
}
KXmlDocument::~KXmlDocument(void) {
	if (cur_node) {
		cur_node->parent = NULL;
		cur_node->release();
	}
	if (root) {
		root->release();
	}
#if 0
	std::list<KXmlNode*>::iterator it;
	for (it = nodeStack.begin(); it != nodeStack.end(); it++) {
		(*it)->release();
	}
#endif
}
KXmlNode* KXmlDocument::getNode(std::string name) {
	KXmlNode* rootNode = getRootNode();
	if (rootNode == NULL) {
		return NULL;
	}
	std::string tag;
	auto pos = name.find('/');
	if (pos != std::string::npos) {
		tag = name.substr(0, pos);
	} else {
		tag = name;
	}
	if (tag != rootNode->key.tag->data) {
		return NULL;
	}
	if (pos == std::string::npos) {
		return rootNode;
	}
	return rootNode->getChild(name.substr(pos + 1));
}
KXmlNode* KXmlDocument::getRootNode() {
	return root;
}
KXmlNode* KXmlDocument::parse(char* str) {
	KXml xml;
	xml.setEvent(this);
	try {
		//printf("%s\n", str);
		xml.startParse(str);
	}
	catch (KXmlException& e) {
		fprintf(stderr, "%s", e.what());
		return nullptr;
	}
	return root;
}
bool KXmlDocument::startElement(KXmlContext* context) {
	//printf("parse node=[%s]\n", context->qName.c_str());
	assert(cur_child_brother == nullptr || cur_child_brother->parent == cur_node);
	cur_node = new KXmlNode(cur_node);
#if 0
	for (auto it = context->attribute.begin(); it != context->attribute.end(); it++) {
		kgl_str_t key;
		key.data = (char*)(*it).first.c_str();
		key.len = (*it).first.size();
		int new_flag;
		auto it2 = cur_node->attributes.m.insert(&key, &new_flag);
		assert(new_flag);
		if (new_flag) {
			it2->value(new KXmlKeyValue(key.data, key.len, (*it).second.c_str(), (*it).second.size()));
		}
	}
#endif
	cur_node->attributes.swap(context->attribute);
	if (!skip_ns) {
		cur_node->key.set_tag(context->qName);
	} else {
		string::size_type pos = context->qName.find(':');
		if (pos != string::npos) {
			//curNode->ns = context->qName.substr(0, pos);
			cur_node->key.set_tag(context->qName.substr(pos + 1));
		} else {
			cur_node->key.set_tag(context->qName);
		}
	}
	if (vary) {
		auto it = vary->find(cur_node->key.tag->data);
		if (it != vary->end()) {
			auto it2 = cur_node->attributes.find((*it).second);
			if (it2 != cur_node->attributes.end()) {
				cur_node->key.vary = kstring_from2((*it2).second.c_str(), (*it2).second.size());
				//cur_node->key.vary = kstring_refs(cur_node->attributes.find((*it).second.c_str(), (*it).second.size()));
			}
		}
	}
	//printf("new xml node=[%p] tag=[%s]\n", cur_node, cur_node->tag.c_str());
	if (cur_child_brother && cur_child_brother->cmp(&cur_node->key) == 0) {
		brothers.push(cur_child_brother);
		//printf("push brother [%s]\n", cur_child_brother->key.tag->data);
	} else {
		brothers.push(nullptr);
		//printf("push null brother\n");	
	}
	cur_child_brother = nullptr;
	return true;

}
bool KXmlDocument::startCharacter(KXmlContext* context, char* character, int len) {
	if (cur_node == NULL) {
		return false;
	}
	assert(cur_node->character == nullptr);
	cur_node->character = kstring_from2(character, len);
	return true;
}
bool KXmlDocument::endElement(KXmlContext* context) {
	if (cur_node == NULL) {
		return false;
	}
	//printf("end node=[%s]\n", context->qName.c_str());
	KXmlNode* parent = cur_node->parent;
	if (!parent) {
		assert(root == nullptr);
		root = cur_node;
	}
	assert(!brothers.empty());
	auto brother = brothers.top();
	brothers.pop();
	if (brother) {
		assert(brother->parent == cur_node->parent);
		assert(kgl_string_cmp(brother->key.tag, cur_node->key.tag) == 0);
		brother->next = cur_node;
	} else {
		if (parent) {
			parent->addChild(cur_node);
		}
	}
	cur_child_brother = cur_node;
	cur_node = parent;
	return true;
}
