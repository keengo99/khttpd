#ifndef KHTTPD_KAUTOARRAY_H
#define KHTTPD_KAUTOARRAY_H
namespace khttpd {
	constexpr uint32_t last_pos{ static_cast<uint32_t>(-1) };
	template <typename T = char>
	class KAutoArray
	{
	public:
		KAutoArray(const KAutoArray& a) = delete;
		KAutoArray() {
			ref = 1;
			count = 0;
			body = nullptr;
		}
		KAutoArray(T* body) {
			ref = 1;			
			this->body = body;
			count = !!body;
		}
		~KAutoArray() {
			if (bodys) {
				assert(count > 0);
				if (count == 1) {
					delete body;
				} else {
					for (uint32_t i = 0; i < count; i++) {
						delete bodys[i];
					}
					xfree(bodys);
				}
			}
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
		T* remove(uint32_t index) {
			if (index >= count) {
				return nullptr;
			}
			if (count == 1) {
				T* body = this->body;
				this->body = nullptr;
				count = 0;
				return body;
			}
			if (count == 2) {
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
		T* remove_last() {
			switch (count) {
			case 0:
				return nullptr;
			case 1:
			{
				T* body = this->body;
				this->body = nullptr;
				count = 0;
				return body;
			}
			case 2:
			{
				auto body = this->bodys[1];
				auto saved_body = this->bodys[0];
				xfree(bodys);
				this->body = saved_body;
				count = 1;
				return body;
			}
			default:
			{
				auto body = this->bodys[count - 1];
				count--;
				return body;
			}
			}
		}
		uint32_t size() {
			return count;
		}
		bool insert(T* body, uint32_t index) {
			if (count == 0) {
				this->body = body;
				count++;
				return true;
			}
			if (count == 1) {
				auto bodys = (T**)malloc(sizeof(T**) * 2);
				if (!bodys) {
					return false;
				}
				bodys[!!index] = body;
				bodys[!index] = this->body;
				this->bodys = bodys;
				count++;
				return true;
			}
			if (!kgl_realloc((void**)&this->bodys, sizeof(T**) * (count + 1))) {
				return false;
			}
			if (index == last_pos || index==count) {
				this->bodys[count] = body;
				count++;
				return true;
			}
			if (index > count) {
				return false;
			}
			memmove(bodys + index + 1, bodys + index, sizeof(T*) * (count - index));
			this->bodys[index] = body;
			count++;
			return true;
		}
		friend class KXmlNode;
	private:
		union
		{
			T* body;
			T** bodys;
		};
		uint32_t count;
		volatile uint32_t ref;
	};
}
#endif


