#ifndef KHTTPD_LOCKER_H
#define KHTTPD_LOCKER_H
#include <exception>
#include "kfiber_sync.h"
class KFiberLocker
{
public:
	KFiberLocker(kfiber_mutex* mutex) {
		if (kfiber_mutex_lock(mutex) != 0) {
			throw std::exception();
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
class KFiberReadLocker
{
public:
	KFiberReadLocker(kfiber_rwlock* mutex) {
		if (kfiber_rwlock_rlock(mutex) != 0) {
			throw std::exception();
		}
		this->mutex = mutex;
	}
	KFiberReadLocker(const KFiberReadLocker& a) = delete;
	KFiberReadLocker(KFiberReadLocker&& a) noexcept {
		this->mutex = a.mutex;
		a.mutex = nullptr;
	}
	~KFiberReadLocker() {
		if (mutex != nullptr) {
			kfiber_rwlock_runlock(mutex);
		}
	}
	KFiberReadLocker& operator=(const KFiberReadLocker& a) = delete;
private:
	kfiber_rwlock* mutex;
};
class KFiberWriteLocker
{
public:
	KFiberWriteLocker(kfiber_rwlock* mutex) {
		if (kfiber_rwlock_wlock(mutex) != 0) {
			throw std::exception();
		}
		this->mutex = mutex;
	}
	KFiberWriteLocker(const KFiberWriteLocker& a) = delete;
	KFiberWriteLocker(KFiberWriteLocker&& a) noexcept {
		this->mutex = a.mutex;
		a.mutex = nullptr;
	}
	~KFiberWriteLocker() {
		if (mutex != nullptr) {
			kfiber_rwlock_wunlock(mutex);
		}
	}
	KFiberWriteLocker& operator=(const KFiberWriteLocker& a) = delete;
private:
	kfiber_rwlock* mutex;
};
#endif
