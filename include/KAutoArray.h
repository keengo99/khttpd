#ifndef KHTTPD_KAUTOARRAY_H
#define KHTTPD_KAUTOARRAY_H
#include <memory>
#include <exception>
#include <stdexcept>
namespace khttpd {
	constexpr uint32_t last_pos{ static_cast<uint32_t>(-1) };

	template <typename T, typename Dy>
	class KAutoArray;

	template <typename T, typename Dy>
	class KAutoArrayIterator
	{
	public:
		KAutoArrayIterator(const KAutoArray<T, Dy>& ar, uint32_t index) : ar(ar) {
			this->index = index;
		}
		KAutoArrayIterator(const KAutoArrayIterator<T, Dy>& it) :ar(it.ar) {
			index = it.index;
		}
		KAutoArrayIterator<T, Dy>& operator++ () {
			++index;
			return *this;
		}
		T* operator *() const {
			return ar.get(index);
		}
		bool operator==(const KAutoArrayIterator<T, Dy>& a) const {
			return index == a.index;
		}
		bool operator!=(const KAutoArrayIterator<T, Dy>& a) const {
			return index != a.index;
		}
		uint32_t get_index() {
			return index;
		}
	private:
		const KAutoArray<T, Dy>& ar;
		uint32_t index;
	};
	template <typename T, typename Dy=std::default_delete<T>>
	class KAutoArray : private Dy
	{
	public:
		using iterator = KAutoArrayIterator<T, Dy>;
		KAutoArray(const KAutoArray& a) = delete;
		KAutoArray() {
			cap = 0;
			count = 0;
			body = nullptr;
		}
		KAutoArray(T* body) {
			cap = 0;			
			this->body = body;
			count = !!body;
		}
		~KAutoArray() {
			if (bodys) {
				assert(count > 0);
				if (count == 1) {
					get_deleter()(body);
				} else {
					for (uint32_t i = 0; i < count; i++) {
						get_deleter()(bodys[i]);
					}
					xfree(bodys);
				}
			}
		}
		iterator begin() {
			return iterator(*this, 0);
		}
		iterator end() {
			return iterator(*this, this->size());
		}
		KAutoArray<T>& operator=(KAutoArray<T>& a) = delete;
		T** get_address(uint32_t index) {
			if (index >= count) {
				return nullptr;
			}
			if (count == 1) {
				return &body;
			}
			return &bodys[index];
		}
		T* first() const {
			switch (count) {
			case 0:
				return nullptr;
			case 1:
				return body;
			default:
				return bodys[0];
			}
		}
		T* get(uint32_t index) const {
			if (index >= count) {
				return nullptr;
			}
			if (count == 1) {
				return body;
			}
			return bodys[index];
		}
		T* last() const {
			switch (count) {
			case 0:
				return nullptr;
			case 1:
				return body;
			default:
				return bodys[count - 1];
			}
		}
		void shrink_to_fit() {
			if (count<2 || count == cap) {
				return;
			}
			if (!kgl_realloc((void**)&this->bodys, sizeof(T**) * (count))) {
				return;
			}
			cap = count;
		}
		KGL_NODISCARD T* remove(uint32_t index) {
			if (index >= count) {
				return nullptr;
			}
			if (count == 1) {
				T* body = this->body;
				this->body = nullptr;
				count = 0;
				assert(cap == 0);
				return body;
			}
			if (count == 2) {
				assert(cap >= 2);
				auto body = this->bodys[index];
				auto saved_body = this->bodys[1 - index];
				xfree(bodys);
				this->body = saved_body;
				count = 1;
				return body;
			}
			auto body = this->bodys[index];
			memmove(bodys + index, bodys + index + 1, sizeof(T*) * (count - index - 1));
			count--;
			return body;
		}
		KGL_NODISCARD T* remove_last() {
			switch (count) {
			case 0:
				return nullptr;
			case 1:
			{
				T* body = this->body;
				this->body = nullptr;
				count = 0;
				assert(cap == 0);
				return body;
			}
			case 2:
			{
				auto body = this->bodys[1];
				auto saved_body = this->bodys[0];
				xfree(bodys);
				this->body = saved_body;
				count = 1;
				assert(cap >= 2);
				cap = 0;
				return body;
			}
			default:
			{
				assert(cap >= count);
				auto body = this->bodys[count - 1];
				count--;
				return body;
			}
			}
		}
		uint32_t size() {
			return count;
		}
		void insert(T* body, uint32_t index) {
			if (count == 0) {
				this->body = body;
				count++;
				assert(cap == 0);
				return;
			}
			if (count == 1) {
				assert(cap == 0);
				cap = 2;
				auto bodys = (T**)malloc(sizeof(T**) * cap);
				if (!bodys) {
					get_deleter()(body);
					throw std::bad_alloc();
					return;
				}
				bodys[!!index] = body;
				bodys[!index] = this->body;
				this->bodys = bodys;
				count++;
				return;
			}
			assert(count <= cap);
			if (count==cap) {
				if (!kgl_realloc((void**)&this->bodys, sizeof(T**) * (cap*2))) {
					get_deleter()(body);
					throw std::bad_alloc();
					return;
				}
				cap *= 2;
			}
			if (index == last_pos || index==count) {
				this->bodys[count] = body;
				count++;
				return;
			}
			if (index > count) {
				get_deleter()(body);
				throw std::out_of_range("out of range");
				return;
			}
			memmove(bodys + index + 1, bodys + index, sizeof(T*) * (count - index));
			this->bodys[index] = body;
			count++;
			return;
		}
		friend class KXmlNode;
	private:
		const Dy& get_deleter() {
			return *this;
		}
		union
		{
			T* body;
			T** bodys;
		};
		uint32_t count;
		uint32_t cap;
	};
}
#endif


