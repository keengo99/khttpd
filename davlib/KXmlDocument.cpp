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
KXmlNode* KXmlDocument::getNode(const std::string& name) {
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
	return rootNode->find_child(name.substr(pos + 1));
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
	auto body = cur_node->get_first();
	body->attributes.swap(context->attribute);
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
				auto it2 = body->attributes.find(key_vary->vary->data);
				if (it2 != body->attributes.end()) {
					cur_node->key.vary = kstring_from2((*it2).second.c_str(), (*it2).second.size());
				} else {
					cur_node->key.vary = kstring_from2(_KS(""));
				}
			}
		}
	}
	cur_node->key.tag = tag;
	return true;

}
bool KXmlDocument::startCharacter(KXmlContext* context, char* character, int len) {
	if (cur_node == NULL) {
		return false;
	}
	auto body = cur_node->get_first();
	if (!body) {
		return false;
	}
	assert(body->character == nullptr);
	body->character = kstring_from2(character, len);
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
	} else {
		parent->append(cur_node);
	}
	cur_node = parent;
	return true;
}
KXmlNodeBody::~KXmlNodeBody() {
	childs.iterator([](void* data, void* arg) {
		KXmlNode* node = (KXmlNode*)data;
		((KXmlNode*)data)->release();
		return iterator_remove_continue;
		}, NULL);
	kstring_release(character);
}
bool KXmlNodeBody::update(KXmlKey* key, uint32_t index, KXmlNode* xml, bool copy_childs, bool create_flag) {
	KMapNode<KXmlNode>* it;
	if (create_flag) {
		if (!xml) {
			return false;
		}
		int new_flag;
		it = childs.insert(key, &new_flag);
		if (new_flag) {
			it->value(xml);
			return true;
		}
	} else {
		it = childs.find(key);
		if (!it) {
			return false;
		}
	}
	auto node = it->value();
	if (!xml) {
		//remove
		auto body = node->remove_body(index);
		if (body) {
			delete body;
			if (node->get_body_count() == 0) {
				node->release();
				childs.erase(it);
			}
			return true;
		}
		return false;
	}
	auto body = node->get_body_address(index);
	if (!body) {
		return false;
	}
	auto xml_body = xml->remove_last();
	if (!xml_body) {
		return false;
	}
	xml->release();
	if (copy_childs) {
		xml_body->copy_child_from(*body);
	}
	delete (*body);
	*body = xml_body;
	return true;
}
void KXmlNodeBody::add(KXmlNode* xml, uint32_t index) {
	int new_flag;
	auto it = childs.insert(&xml->key, &new_flag);
	if (new_flag) {
		it->value(xml);
		return;
	}
	auto old_node = it->value();
	while (auto body = xml->remove_last()) {
		if (!old_node->insert_body(body, index)) {
			delete body;
		}
	}
	xml->release();
}
void KXmlNodeBody::copy_child_from(KXmlNodeBody* node) {
	for (auto it = node->childs.first(); it; it = it->next()) {
		add(it->value()->add_ref(), KXmlNode::last_pos);
	}
}
KGL_RESULT KXmlNodeBody::write(KWStream* out, int level) {
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
			//klog(KLOG_ERR, "cann't write xml attribute [%s] value also has ['\"]\n", (*it).first.c_str());
			out->write_all(_KS("''"));
		}
	}
	//write attribute
	out->write_all(_KS(">"));
	//write child
	if (!childs.empty()) {
		out->write_all(_KS("\n"));
	}
	for (auto it = childs.first(); it; it = it->next()) {
		auto result = it->value()->write(out, level + 1);
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
		if (!childs.empty()) {
			out->write_all(_KS("\n"));
		}
	}
	if (!childs.empty()) {
		for (int i = 0; i < level; i++) {
			out->write_all(_KS("\t"));
		}
	}
	return KGL_OK;
}
KXmlNodeBody* KXmlNodeBody::clone() {
	KXmlNodeBody* node = new KXmlNodeBody;
	node->attributes = attributes;
	node->character = kstring_refs(character);
	for (auto it = childs.first(); it; it = it->next()) {
		node->add(it->value()->clone(), KXmlNode::last_pos);
	}
	return node;
}
KXmlNode* KXmlNodeBody::find_child(const std::string& tag) {
	size_t pos = tag.find('/');
	std::string child_tag;
	if (pos != std::string::npos) {
		child_tag = tag.substr(0, pos);
	} else {
		child_tag = tag;
	}
	KXmlKey key(child_tag.c_str(), child_tag.size());
	auto it = childs.find(&key);
	if (!it) {
		return nullptr;
	}
	if (pos != std::string::npos) {
		return it->value()->find_child(tag.substr(pos + 1));
	}
	return it->value();
}