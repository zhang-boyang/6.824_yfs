// this is the lock server
// the lock client has a similar interface

#ifndef lock_server_h
#define lock_server_h

#include <string>
#include "lock_protocol.h"
#include "lock_client.h"
#include "rpc.h"

struct DataControl {
  pthread_mutex_t pmutex;
};

struct CondLock {
	CondLock() {
		is_locked = false;
		pthread_cond_init(&pcond, NULL);
	}
	virtual ~CondLock() {
		pthread_cond_destroy(&pcond);
	}
	bool is_locked;
	pthread_cond_t pcond;
};

class lock_server {

 protected:
  int nacquire;
  std::map<lock_protocol::lockid_t, CondLock> lock_map_;
  DataControl pcontrol_;
 public:
  lock_server();
  ~lock_server();
  lock_protocol::status stat(int clt, lock_protocol::lockid_t lid, int &r);
  lock_protocol::status acquire_lock(int clt, lock_protocol::lockid_t lid, int &r);
  lock_protocol::status release_lock(int clt, lock_protocol::lockid_t lid, int &r);
};

#endif 






