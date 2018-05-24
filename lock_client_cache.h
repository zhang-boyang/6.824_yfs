// lock client interface.

#ifndef lock_client_cache_h

#define lock_client_cache_h

#include <string>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_client.h"
#include "lang/verify.h"
#include "extent_client_cache.h"
// Classes that inherit lock_release_user can override dorelease so that 
// that they will be called when lock_client releases a lock.
// You will not need to do anything with this class until Lab 5.
class lock_release_user {
 public:
  virtual void dorelease(lock_protocol::lockid_t) = 0;
  virtual ~lock_release_user() {};
};

class lock_release_user_dev : public lock_release_user {
 private:
    extent_client_cache *ec_;
 public:
    lock_release_user_dev(extent_client_cache *ec):
    ec_(ec){}
    void dorelease(lock_protocol::lockid_t lid){
        ec_->flush(lid);
    }
};

class ClientLock{
  public:
	pthread_cond_t wait_acq_;
	pthread_cond_t wait_free_;
	bool is_retried;
	bool is_revoked;
	bool is_finished;
	AcqRet::lock_status status;
	ClientLock():
	    is_retried(false),
	    is_revoked(false),
	    is_finished(false),
	    status(AcqRet::NONE){
	    pthread_cond_init(&wait_acq_, NULL);
	    pthread_cond_init(&wait_free_, NULL);
	}
};


class lock_client_cache : public lock_client {
 private:
  class lock_release_user *lu;
  int rlock_port;
  std::string hostname;
  std::string id;
  pthread_mutex_t lock_map_mutex_;
  std::map<lock_protocol::lockid_t, ClientLock> m_lock_map_;
 public:
  lock_client_cache(std::string xdst, class lock_release_user *l = 0);
  virtual ~lock_client_cache();
  lock_protocol::status acquire(lock_protocol::lockid_t);
  lock_protocol::status release(lock_protocol::lockid_t);
  rlock_protocol::status revoke_handler(lock_protocol::lockid_t, 
                                        int &);
  rlock_protocol::status retry_handler(lock_protocol::lockid_t, 
                                       int &);

  void DealWithStatusACQ    (ClientLock & , lock_protocol::lockid_t);
  void DealWithStatusFREE   (ClientLock & , lock_protocol::lockid_t);
  void DealWithStatusLOCK   (ClientLock & , lock_protocol::lockid_t);
  void DealWithStatusNONE   (ClientLock & , lock_protocol::lockid_t);
  int SendAcqToSvr         (ClientLock & , lock_protocol::lockid_t);
  void DealRelease(ClientLock & , lock_protocol::lockid_t);
};


#endif
