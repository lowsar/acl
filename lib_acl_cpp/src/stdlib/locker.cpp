#include "acl_stdafx.hpp"
#include "acl_cpp/stdlib/locker.hpp"

namespace acl {

locker::locker(bool use_mutex /* = true */, bool nowait /* = false */)
	: pFile_(NULL)
	, myFHandle_(false)
	, nowait_(nowait)
{
	fHandle_ = ACL_FILE_INVALID;
	if (use_mutex)
		init_mutex();
	else
		pMutex_ = NULL;
}

locker::~locker()
{
	if (pFile_)
		acl_myfree(pFile_);
	if (myFHandle_ && fHandle_ != ACL_FILE_INVALID)
		acl_file_close(fHandle_);
	if (pMutex_)
	{
#ifndef WIN32
		pthread_mutexattr_destroy(&mutexAttr_);
#endif
		acl_pthread_mutex_destroy(pMutex_);
		acl_myfree(pMutex_);
	}
}

void locker::init_mutex()
{
	pMutex_ = (acl_pthread_mutex_t*)
		acl_mycalloc(1, sizeof(acl_pthread_mutex_t));
#ifndef WIN32
	pthread_mutexattr_init(&mutexAttr_);
	pthread_mutexattr_settype(&mutexAttr_, PTHREAD_MUTEX_RECURSIVE);
	acl_pthread_mutex_init(pMutex_, &mutexAttr_);
#else
	acl_pthread_mutex_init(pMutex_, NULL);
#endif
}

bool locker::open(const char* file_path)
{
	acl_assert(file_path && *file_path);
	acl_assert(pFile_ == NULL);
	acl_assert(fHandle_ == ACL_FILE_INVALID);

	fHandle_ = acl_file_open(file_path, O_RDWR | O_CREAT, 0600);
	if (fHandle_ == ACL_FILE_INVALID)
		return false;
	myFHandle_ = true;
	pFile_ = acl_mystrdup(file_path);
	return true;
}

bool locker::open(ACL_FILE_HANDLE fh)
{
	acl_assert(myFHandle_ == false);
	fHandle_ = fh;
	return true;
}

bool locker::lock()
{
	if (pMutex_)
	{
		if (nowait_)
		{
			if (acl_pthread_mutex_trylock(pMutex_) == -1)
				return false;
		}
		else if (acl_pthread_mutex_lock(pMutex_) == -1)
			return false;
	}

	if (fHandle_ != ACL_FILE_INVALID)
	{
		int operation = ACL_FLOCK_OP_EXCLUSIVE;
		if (nowait_)
			operation |= ACL_FLOCK_OP_NOWAIT;
		if (acl_myflock(fHandle_, ACL_FLOCK_STYLE_FCNTL,
			operation) == -1)
		{
			return false;
		}
	} else if (pFile_ != NULL)
		return false;
	return true;
}

bool locker::unlock()
{
	bool  ret;

	if (pMutex_)
	{
		if (acl_pthread_mutex_unlock(pMutex_) == -1)
			ret = false;
		else
			ret = true;
	}

	if (fHandle_ != ACL_FILE_INVALID)
	{
		if (acl_myflock(fHandle_, ACL_FLOCK_STYLE_FCNTL,
			ACL_FLOCK_OP_NONE) == -1)
		{
			ret = false;
		}
		else
			ret = true;
	} else if (pFile_ != NULL)
		ret = false;
	else
		ret = true;
	return (ret);
}

} // namespace acl
