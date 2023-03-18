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
#include "klist.h"
#include "KSharedObj.h"

namespace khttpd {
	constexpr char internal_xml_attribute = '_';
	class KXmlKey
	{
	public:
		KXmlKey(const char* tag, size_t size, uint32_t tag_id = 0) {
			ref = 1;
			this->tag_id = tag_id;
			const char* p = (char*)memchr(tag, '@', size);
			if (!p) {
				this->tag = kstring_from2(tag, size);
				this->vary = nullptr;
				return;
			}
			this->tag = kstring_from2(tag, p - tag);
			this->vary = kstring_from2(p + 1, size - this->tag->len - 1);
		}
		KXmlKey(kgl_ref_str_t* tag, kgl_ref_str_t* vary, uint32_t tag_id = 0) {
			this->tag = tag;
			this->vary = vary;
			this->tag_id = tag_id;
			ref = 1;
		}
		KXmlKey() {
			tag = nullptr;
			vary = nullptr;
			ref = 1;
			tag_id = 0;
		}
		~KXmlKey() {
			kstring_release(tag);
			kstring_release(vary);
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
		uint32_t tag_id;
		volatile uint32_t ref;
	};
	class KXmlNode;
	using KSafeXmlNode = KSharedObj<KXmlNode>;
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
			return attributes == node->attributes;
		}
		const char* get_text(const char* default_text = "") const {
			auto it = attributes.find(text_as_attribute_name);
			if (it == attributes.end()) {
				return default_text;
			}
			return (*it).second.c_str();
		}
		KString get_character() const {
			auto it = attributes.find(text_as_attribute_name);
			if (it == attributes.end()) {
				return KXmlAttribute::empty;
			}
			return (*it).second;
		}
		void set_text(const KString& value) {
			auto result = attributes.emplace(text_as_attribute_name, value);
			if (!result.second) {
				(*(result.first)).second = value;
			}
		}
		KXmlNode* find_child(const KXmlKey* a) const {
			auto it = childs.find(a);
			if (!it) {
				return nullptr;
			}
			return it->value();
		}
		const KXmlAttribute& attr() const {
			return attributes;
		}
		KXmlAttribute& attr() {
			return attributes;
		}
		void copy_child_from(const KXmlNodeBody* node);
		KXmlNode* find_child(const KString& tag) const;
		void clear() {
			childs.clear();
			attributes.clear();
		}
		bool update(KXmlKey* key, uint32_t index, KXmlNode* xml, bool copy_childs, bool create_flag);
		KMap<KXmlKey, KXmlNode> childs;
		KXmlAttribute attributes;
		KXmlNodeBody* add(KXmlNode* xml, uint32_t index);
		friend class KXmlNode;
		static const KString text_as_attribute_name;
	private:

	};
	using KSafeXmlNodeBody = std::unique_ptr<KXmlNodeBody>;
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
			key.tag_id = node->key.tag_id;

			for (uint32_t index = 0;; index++) {
				auto body = node->get_body(index);
				if (!body) {
					break;
				}
				insert_body(body->clone(), khttpd::last_pos);
			}
		}
		KXmlNode(kgl_ref_str_t* tag, kgl_ref_str_t* vary, uint32_t tag_id) : key(tag, vary, tag_id), body(new KXmlNodeBody) {
		}
		KXmlNode(KXmlKey* name) : key(kstring_refs(name->tag), kstring_refs(name->vary), name->tag_id), body(new KXmlNodeBody) {
		}
		int cmp(const KXmlKey* a)  const {
			int result = (int)a->tag_id - (int)key.tag_id;
			if (result != 0) {
				return result;
			}
			if (key.tag_id != 0) {
				//know id
				return kgl_string_cmp(key.vary, a->vary);
			}
			return key.cmp(a);
		}

		bool is_tag(const char* tag, size_t len) const {
			return kgl_cmp(key.tag->data, key.tag->len, tag, len) == 0;
		}
		KXmlNode* find_child(const KString& tag) const {
			auto body = get_first();
			if (!body) {
				return nullptr;
			}
			return body->find_child(tag);
		}
		KSafeXmlNode clone() const {
			return KSafeXmlNode(new KXmlNode(this));
		}
		KXmlNodeBody* get_body(uint32_t index) const {
			return body.get(index);
		}
		KGL_NODISCARD KXmlNode* add_ref() {
			katom_inc((void*)&key.ref);
			return this;
		}
		void release() {
			assert(katom_get((void*)&key.ref) < 0xfffffff);
			if (katom_dec((void*)&key.ref) == 0) {
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
		KString get_tag() const {
			return KString(kstring_refs(key.tag));
		}
		KString get_character() const {
			auto body = get_first();
			if (!body) {
				return KXmlAttribute::empty;
			}
			return body->get_character();
		}
		const char* get_text() const {
			auto body = get_first();
			if (!body) {
				return "";
			}
			return body->get_text("");
		}
		KXmlAttribute& attributes() const {
			return get_first()->attributes;
		}
		KGL_NODISCARD KXmlNodeBody* remove_last() {
			return body.remove_last();
		}
		KXmlNodeBody* remove_body(uint32_t index) {
			return body.remove(index);
		}
		void insert_body(KXmlNodeBody* body, uint32_t index) {
			for (auto it = body->childs.first(); it; it = it->next()) {
				it->value()->body.shrink_to_fit();
			}
			this->body.insert(body, index);
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
		khttpd::KAutoArray<KXmlNodeBody> body;
		friend class KXmlNodeBody;
	private:
		~KXmlNode() {
		}
	};

	class KXmlDocument : public KXmlEvent
	{
	public:
		KXmlDocument(bool skip_ns = true);
		~KXmlDocument(void);
		void set_qname_config(KMap<kgl_ref_str_t, KXmlKey>* qname_config) {
			this->qname_config = qname_config;
		}
		KSafeXmlNode parse(char* str);
		KXmlNode* getRootNode() const;
		KXmlNode* getNode(const std::string& name) const;
		bool startElement(KXmlContext* context) override;
		bool startCharacter(KXmlContext* context, char* character, int len) override;
		bool endElement(KXmlContext* context) override;
	private:
		bool skip_ns;
		khttpd::KSafeXmlNode cur_node;
		khttpd::KSafeXmlNode root;
		std::stack<khttpd::KSafeXmlNode> parents;
		KMap<kgl_ref_str_t, KXmlKey>* qname_config = nullptr;
	};

}
#endif
