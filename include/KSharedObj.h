#ifndef KHTTPD_SHARED_OBJ_INCLUDE
#define KHTTPD_SHARED_OBJ_INCLUDE
#include <cstddef>
#include <memory>
template<class T>
struct KSharedObjDeleter
{
    void operator()(T* t) const noexcept {
        t->release();
    }
};
template<class T>
class KSharedObj : public std::unique_ptr<T, KSharedObjDeleter<T>>
{
public:
    using unique_ptr = std::unique_ptr<T, KSharedObjDeleter<T>>;
    constexpr KSharedObj(std::nullptr_t) noexcept : unique_ptr(nullptr) {
    }
    explicit KSharedObj(T* a) : unique_ptr(a) {
    }
    constexpr KSharedObj() noexcept : unique_ptr() {
    }
    KSharedObj(KSharedObj&& a) noexcept : unique_ptr(std::forward<unique_ptr>(a)) {
    }

    template<class DT=T>
    KSharedObj(KSharedObj<DT>&& a) noexcept : unique_ptr(static_cast<T*>(a.release())) {
    }
    template<class DT>
    KSharedObj(const KSharedObj<DT> &a) noexcept : unique_ptr(static_cast<T*>(a?a->add_ref():nullptr)) {
    }
    KSharedObj(const KSharedObj& a) noexcept : unique_ptr(static_cast<T *>(a?a->add_ref():nullptr)) {
    }
    KSharedObj& operator=(const KSharedObj& a) {
        if (!a) {
            unique_ptr::reset(nullptr);
            return *this;
        }
        unique_ptr::reset(static_cast<T *>(a->add_ref()));
        return *this;
    }
};
#endif
