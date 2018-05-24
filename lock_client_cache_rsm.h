// lock client interface.

#ifndef lock_client_cache_rsm_h

#define lock_client_cache_rsm_h

#include <string>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_client.h"
#include "lang/verify.h"

#include "rsm_client.h"

// Classes that inherit lock_release_user can override dorelease so that 
// that they will be called when lock_client releases a lock.
// You will not need to do anything with this class until Lab 5.
class lock_release_user {
 public:
  virtual void dorelease(lock_protocol::lockid_t) = 0;
  virtual ~lock_release_user() {};
};


class ClientLockRSM {
public:
  pthread_cond_t wait_acq_;
  pthread_cond_t wait_free_;
  pthread_cond_t wait_release_;
  bool is_retried;
  bool is_revoked;
  bool is_finished;
  AcqRet::lock_status status;
  lock_protocol::lockid_t lid;
  lock_protocol::xid_t xid;
  ClientLockRSM():
      is_retried(false),
      is_revoked(false),
      is_finished(false),
      status(AcqRet::NONE),
      lid(0), xid(0){
      pthread_cond_init(&wait_acq_, NULL);
      pthread_cond_init(&wait_free_, NULL);
      pthread_cond_init(&wait_release_, NULL);
  }
};

// Clients that caches locks.  The server can revoke locks using 
// lock_revoke_server.
class lock_client_cache_rsm : public lock_client {
 private:
  rsm_client *rsmc;
  class lock_release_user *lu;
  int rlock_port;
  std::string hostname;
  std::string id;
  lock_protocol::xid_t xid;
  std::map<lock_protocol::lockid_t, ClientLockRSM> map_lock_;
  pthread_mutex_t lock_map_mutex_;
  fifo<ClientLockRSM*> release_queue_;
 public:
  static int last_port;
  lock_client_cache_rsm(std::string xdst, class lock_release_user *l = 0);
  virtual ~lock_client_cache_rsm();
  lock_protocol::status acquire(lock_protocol::lockid_t);
  virtual lock_protocol::status release(lock_protocol::lockid_t);
  void releaser();
  rlock_protocol::status revoke_handler(lock_protocol::lockid_t, 
				        lock_protocol::xid_t, int &);
  rlock_protocol::status retry_handler(lock_protocol::lockid_t, 
				       lock_protocol::xid_t, int &);
  void DealWithStatusACQ    (ClientLockRSM & , lock_protocol::lockid_t);
  void DealWithStatusFREE   (ClientLockRSM & , lock_protocol::lockid_t);
  void DealWithStatusLOCK   (ClientLockRSM & , lock_protocol::lockid_t);
  void DealWithStatusNONE   (ClientLockRSM & , lock_protocol::lockid_t);
  int SendAcqToSvr         (ClientLockRSM & , lock_protocol::lockid_t);
  void DealRelease(ClientLockRSM & , lock_protocol::lockid_t);
};


#endif
