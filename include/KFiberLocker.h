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
	~KFiberLocker() {
		kfiber_mutex_unlock(mutex);
	}
private:
	kfiber_mutex* mutex;
};
#endif