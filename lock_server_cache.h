#ifndef lock_server_cache_h
#define lock_server_cache_h

#include <string>

#include <map>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_server.h"
#include <queue>
struct CondLockCache : public CondLock{
	CondLockCache() {
	}
	~CondLockCache() {
	}
	std::string user_id_;
};

class lock_server_cache {
 private:
  int nacquire;
 public:
  pthread_mutex_t m_lock_mutex_;
  pthread_mutex_t m_retry_mutex_;
  std::map<lock_protocol::lockid_t, CondLockCache> m_map_lock_;
  std::map<lock_protocol::lockid_t, std::queue<std::string> > m_retry_queue_;
  lock_server_cache();
  ~lock_server_cache();
  lock_protocol::status stat(lock_protocol::lockid_t, int &);
  int acquire(lock_protocol::lockid_t, std::string id, int &);
  int release(lock_protocol::lockid_t, std::string id, int &);
};

class ConnectToClient {
 public:
	rpcc *cl;
	std::string id;
	ConnectToClient(std::string user_id);
	virtual ~ConnectToClient();
	virtual lock_protocol::status Revoke(lock_protocol::lockid_t);
	virtual lock_protocol::status Retry(lock_protocol::lockid_t);
};

#endif
