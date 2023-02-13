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
		cur_node->release();
	}
	if (root) {
		root->release();
	}
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
	parents.push(cur_node);
	cur_node = new KXmlNode();
	cur_node->attributes.swap(context->attribute);
	kgl_ref_str_t* tag;
	if (!skip_ns) {
		tag = kstring_from2(context->qName.c_str(), context->qName.size());
	} else {
		auto pos = context->qName.find(':');
		if (pos != std::string::npos) {
			auto q_name = context->qName.substr(pos + 1);
			tag = kstring_from2(q_name.c_str(), q_name.size());
		} else {
			tag = kstring_from2(context->qName.c_str(), context->qName.size());
		}
	}

	if (qname_config) {
		auto it = qname_config->find(tag);
		if (it) {
			auto key_vary = it->value();
			kstring_release(tag);
			tag = kstring_refs(key_vary->tag);
			if (key_vary->vary) {
				auto it2 = cur_node->attributes.find(key_vary->vary->data);
				if (it2 != cur_node->attributes.end()) {
					cur_node->key.vary = kstring_from2((*it2).second.c_str(), (*it2).second.size());
				} else {
					cur_node->key.vary = kstring_from2(_KS(""));
				}
			}
		}
	}
	cur_node->key.tag = tag;
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
	KXmlNode* parent = nullptr;
	if (!parents.empty()) {
		parent = parents.top();
		parents.pop();
	}
	if (!parent) {
		assert(root == nullptr);
		root = cur_node;
	}
	assert(!brothers.empty());
	auto brother = brothers.top();
	brothers.pop();
	if (brother) {
		assert(kgl_string_cmp(brother->key.tag, cur_node->key.tag) == 0);
		brother->next = cur_node;
	} else {
		if (parent) {
			parent->append(cur_node);
		}
	}
	cur_child_brother = cur_node;
	cur_node = parent;
	return true;
}
