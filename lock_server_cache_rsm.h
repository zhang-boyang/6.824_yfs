#ifndef lock_server_cache_rsm_h
#define lock_server_cache_rsm_h

#include <string>

#include "lock_protocol.h"
#include "rpc.h"
#include "rsm_state_transfer.h"
#include "rsm.h"
#include "lock_server.h"
#include "fifo.h"
#include <queue>

#define foreach(item, it) for(typeof((item).begin()) it = (item).begin(); it != (item).end(); it++)

struct CondLockCacheRSM : public CondLock{
    //Recode every user_id with it's highest
    std::map<std::string, lock_protocol::xid_t> lock_info_;
    std::queue<std::string> wait_retry_queue_;
    std::queue<std::string> wait_revoke_queue_;
    lock_protocol::lockid_t lid;
    std::string user_id;
};


class lock_server_cache_rsm : public rsm_state_transfer {
 private:
  int nacquire;
  class rsm *rsm;
  std::map<lock_protocol::lockid_t, CondLockCacheRSM> map_lock_;
  pthread_mutex_t m_lock_mutex_;
  //pthread_mutex_t m_lock_retry_;
  //pthread_mutex_t m_lock_release_;
  fifo<CondLockCacheRSM *> retry_queue_;
  fifo<CondLockCacheRSM *> revoke_queue_;
 public:
  lock_server_cache_rsm(class rsm *rsm = 0);
  lock_protocol::status stat(lock_protocol::lockid_t, int &);
  void revoker();
  void retryer();
  std::string marshal_state();
  void unmarshal_state(std::string state);
  int acquire(lock_protocol::lockid_t, std::string id, 
	      lock_protocol::xid_t, int &);
  int release(lock_protocol::lockid_t, std::string id, lock_protocol::xid_t,
	      int &);
};

#endif
