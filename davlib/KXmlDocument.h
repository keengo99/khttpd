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
#include "KAutoArray.h"
namespace khttpd {
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
		KXmlKey(kgl_ref_str_t* tag, kgl_ref_str_t* vary) {
			this->tag = tag;
			this->vary = vary;
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
		bool is_wide() const {
			return *(tag->data) == '*';
		}
		int cmp(const kgl_ref_str_t* key) const {
			return kgl_cmp(tag->data, tag->len, key->data, key->len);
		}
		int cmp(const KXmlKey* a) const {
			int ret = cmp(a->tag);
			if (ret != 0) {
				return ret;
			}
			return kgl_string_cmp(vary, a->vary);
		}
		kgl_ref_str_t* tag;
		kgl_ref_str_t* vary;
	};
	class KXmlNode;
	class KXmlNodeBody
	{
	public:
		KXmlNodeBody() {

		}
		KXmlNodeBody(const KXmlNodeBody& a) = delete;
		~KXmlNodeBody();
		KXmlNodeBody* clone() const;
		KGL_RESULT write(KWStream* out, int level) const;
		bool is_same(KXmlNodeBody* node) const {
			if (kgl_string_cmp(character, node->character) != 0) {
				return false;
			}
			return attributes == node->attributes;
		}
		const char* get_text(const char* default_text) const {
			return character ? character->data : default_text;
		}
		const std::string get_character() const {
			return get_text("");
		}
		KXmlNode* find_child(KXmlKey* a) const {
			auto it = childs.find(a);
			if (!it) {
				return nullptr;
			}
			return it->value();
		}
		void copy_child_from(const KXmlNodeBody* node);
		KXmlNode* find_child(const std::string& tag) const;
		void add(KXmlNode* xml, uint32_t index);
		bool update(KXmlKey* key, uint32_t index, KXmlNode* xml, bool copy_childs, bool create_flag);
		KMap<KXmlKey, KXmlNode> childs;
		KXmlAttribute attributes;
		kgl_ref_str_t* character = nullptr;
	};
	class KXmlNode
	{
	public:
		KXmlNode(const KXmlNode& node) = delete;
		KXmlNode(const KXmlNode* node = nullptr) {
			if (!node) {
				body.body = new KXmlNodeBody;
				body.count = 1;
				return;
			}
			key.tag = kstring_refs(node->key.tag);
			key.vary = kstring_refs(node->key.vary);
			for (uint32_t index = 0;; index++) {
				auto body = node->get_body(index);
				if (!body) {
					break;
				}
				insert_body(body->clone(), khttpd::last_pos);
			}
		}
		KXmlNode(kgl_ref_str_t* tag, kgl_ref_str_t* vary) : key(tag, vary), body(new KXmlNodeBody) {

		}
		KXmlNode(KXmlKey* name) : key(kstring_refs(name->tag), kstring_refs(name->vary)), body(new KXmlNodeBody) {
		}
		int cmp(const KXmlKey* a)  const {
			int result = (int)key.tag->id - (int)a->tag->id;
			if (result != 0) {
				return result;
			}
			if (key.tag->id != 0) {
				//know id
				return kgl_string_cmp(key.vary, a->vary);
			}
			return key.cmp(a);
		}

		bool is_tag(const char* tag, size_t len) const {
			return kgl_cmp(key.tag->data, key.tag->len, tag, len) == 0;
		}
		KXmlNode* find_child(const std::string& tag) const {
			auto body = get_first();
			if (!body) {
				return nullptr;
			}
			return body->find_child(tag);
		}
		KXmlNode* clone() const {
			return new KXmlNode(this);
		}
		KXmlNodeBody* get_body(uint32_t index) const {
			return body.get(index);
		}
		KXmlNode* add_ref() {
			katom_inc((void*)&body.ref);
			return this;
		}
		void release() {
			assert(katom_get((void*)&body.ref) < 0xfffffff);
			if (katom_dec((void*)&body.ref) == 0) {
				delete this;
			}
		}
		KGL_RESULT write(KWStream* out, int level = 0) const {
			for (uint32_t index = 0;; index++) {
				auto body = get_body(index);
				if (!body) {
					break;
				}
				for (int i = 0; i < level; i++) {
					out->write_all(_KS("\t"));
				}
				out->write_all(_KS("<"));
				out->write_all(key.tag->data, key.tag->len);
				auto result = body->write(out, level);
				if (result == KGL_END) {
					continue;
				}
				if (result != KGL_OK) {
					return result;
				}
				out->write_all(_KS("</"));
				out->write_all(key.tag->data, key.tag->len);
				result = out->write_all(_KS(">\n"));
				if (result != KGL_OK) {
					return result;
				}
			}
			return KGL_OK;
		}

		/* return false xml must call release. otherwise xml do not call release */
		bool update(KXmlKey* key, uint32_t index, KXmlNode* xml, bool copy_childs = true, bool create_flag = false) {
			auto body = get_first();
			if (!body) {
				return false;
			}
			return body->update(key, index, xml, copy_childs, create_flag);
		}
		bool append(KXmlNode* xml) {
			auto body = get_last();
			assert(body);
			if (!body) {
				return false;
			}
			body->add(xml, khttpd::last_pos);
			return true;
		}
		/* insert before index node */
		bool insert(KXmlNode* xml, uint32_t index) {
			auto body = get_first();
			assert(body);
			if (!body) {
				return false;
			}
			body->add(xml, index);
			return true;
		}
		const std::string get_tag() const {
			return key.tag->data;
		}
		const std::string get_character() const {
			auto body = get_first();
			if (!body) {
				return "";
			}
			return body->get_character();
		}
		const char* get_text() const {
			auto body = get_first();
			if (!body || !body->character) {
				return "";
			}
			return body->character->data;
		}
		KXmlAttribute& attributes() const {
			return get_first()->attributes;
		}
		KXmlNodeBody* remove_last() {
			return body.remove_last();
		}
		KXmlNodeBody* remove_body(uint32_t index) {
			return body.remove(index);
		}
		bool insert_body(KXmlNodeBody* body, uint32_t index) {
			return this->body.insert(body, index);
		}
		KXmlNodeBody* get_last() const {
			return body.last();
		}
		KXmlNodeBody* get_first() const {
			return body.first();
		}
		KXmlNodeBody** get_body_address(uint32_t index) {
			return body.get_address(index);
		}

		uint32_t get_body_count() const {
			return body.count;
		}
		KXmlKey key;
	private:
		khttpd::KAutoArray<KXmlNodeBody> body;
		~KXmlNode() {
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
		KXmlNode* getRootNode() const;
		KXmlNode* getNode(const std::string& name) const;
		bool startElement(KXmlContext* context) override;
		bool startCharacter(KXmlContext* context, char* character, int len) override;
		bool endElement(KXmlContext* context) override;
	private:
		bool skip_ns;
		KXmlNode* cur_node = nullptr;
		KXmlNode* root = nullptr;
		std::stack<KXmlNode*> parents;
		KMap<kgl_ref_str_t, KXmlKey>* qname_config = nullptr;
	};
}
#endif
