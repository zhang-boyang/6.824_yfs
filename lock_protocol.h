// lock protocol

#ifndef lock_protocol_h
#define lock_protocol_h

#include "rpc.h"

class lock_protocol {
 public:
  enum xxstatus { OK, RETRY, RPCERR, NOENT, IOERR };
  typedef int status;
  typedef unsigned long long lockid_t;
  typedef unsigned long long xid_t;
  enum rpc_numbers {
    acquire = 0x7001,
    release,
    stat
  };
};

class rlock_protocol {
 public:
  enum xxstatus { OK, RPCERR };
  typedef int status;
  enum rpc_numbers {
    revoke = 0x8001,
    retry = 0x8002
  };
};
class AcqRet {
public:
	enum status {OK = 0, RETRY};
	enum lock_status {NONE, FREE, LOCK, RELEASING, ACQING};
};


class ScopeLock{
  public:
	ScopeLock(pthread_mutex_t* mutex)
			:mutex_(mutex){
		pthread_mutex_lock(mutex_);
	}
	~ScopeLock(){
		pthread_mutex_unlock(mutex_);
	}
  private:
	pthread_mutex_t* mutex_;
};
#endif 
