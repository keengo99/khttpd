#ifndef KHTTPD_LOCKER_H
#define KHTTPD_LOCKER_H
#include <exception>
#include "kfiber_sync.h"
class KFiberLocker
{
public:
	KFiberLocker(kfiber_mutex* mutex) {
		if (kfiber_mutex_lock(mutex) != 0) {
			throw std::exception("lock failed.");
		}
		this->mutex = mutex;
	}
	KFiberLocker(const KFiberLocker& a) = delete;
	KFiberLocker(KFiberLocker&& a) noexcept {
		this->mutex = a.mutex;
		a.mutex = nullptr;
	}
	~KFiberLocker() {
		if (mutex != nullptr) {
			kfiber_mutex_unlock(mutex);
		}
	}
	KFiberLocker &operator=(const KFiberLocker& a) = delete;
private:
	kfiber_mutex* mutex;
};
#endif