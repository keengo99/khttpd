#ifndef KMAP_H_INCLUDED
#define KMAP_H_INCLUDED
#include "krbtree.h"

template <typename Value>
class KMapNode : public krb_node
{
public:
	inline Value* value() const {
		return (Value*)data;
	}
	inline void value(Value* data) {
		this->data = data;
	}
	inline KMapNode<Value>* next() const {
		return (KMapNode<Value> *)rb_next(this);
	}
	inline KMapNode<Value>* prev() const {
		return (KMapNode<Value> *)rb_prev(this);
	}
};
template <typename Key, typename Value>
class KMap
{
public:
	KMap() {
		rbtree_init(&tree);
	}
	~KMap() {
		assert(rbtree_is_empty(&tree));
		clear();
	}
	static int cmp_func(const void* k1, const void* k2) {
		return ((const Value*)k2)->cmp((const Key*)k1);
	}
	inline bool empty() const {
		return rbtree_is_empty(&tree);
	}
	void clear() {
		rbtree_iterator(&tree, [](void* data, void* arg)->iterator_ret {
			return iterator_remove_continue;
			}, NULL);
	}
	inline KMapNode<Value>* find(const Key* key) const {
		return (KMapNode<Value> *)rbtree_find(&tree, key, cmp_func);
	}
	inline KMapNode<Value>* insert(Key* key, int* new_flag) {
		return (KMapNode<Value> *)rbtree_insert(&tree, key, new_flag, cmp_func);
	}
	inline Value *add(Key* key, Value* value) {
		int new_flag;
		auto it = insert(key, &new_flag);
		if (!new_flag) {
			auto ret = it->value();
			it->value(value);
			return ret;
		}
		it->value(value);
		return nullptr;
	}
	inline KMapNode<Value>* last() const {
		return (KMapNode<Value> *)rb_last(&tree.root);
	}
	inline KMapNode<Value>* first() const {
		return (KMapNode<Value> *)rb_first(&tree.root);
	}
	inline void iterator(iteratorbt bt, void* arg) {
		rbtree_iterator(&tree, bt, arg);
	}
	inline void erase(KMapNode<Value>* node) {
		rbtree_remove(&tree, node);
	}
	void swap(KMap<Key, Value>* a) {
		struct krb_tree tmp;
		memcpy(&tmp, &tree, sizeof(struct krb_tree));
		memcpy(&tree, &a->tree, sizeof(struct krb_tree));
		memcpy(&a->tree, &tmp, sizeof(struct krb_tree));
	}
private:
	struct krb_tree tree;
};
#endif