// the caching lock server implementation

#include "lock_server_cache_rsm.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "lang/verify.h"
#include "handle.h"
#include "tprintf.h"


static void *
revokethread(void *x)
{
  lock_server_cache_rsm *sc = (lock_server_cache_rsm *) x;
  sc->revoker();
  return 0;
}

static void *
retrythread(void *x)
{
  lock_server_cache_rsm *sc = (lock_server_cache_rsm *) x;
  sc->retryer();
  return 0;
}

lock_server_cache_rsm::lock_server_cache_rsm(class rsm *_rsm) 
  : rsm (_rsm)
{
  pthread_t th;
  int r = pthread_create(&th, NULL, &revokethread, (void *) this);
  VERIFY (r == 0);
  r = pthread_create(&th, NULL, &retrythread, (void *) this);
  VERIFY (r == 0);
  nacquire = 0;
  pthread_mutex_init(&m_lock_mutex_, NULL);
}

void
lock_server_cache_rsm::revoker()
{

  // This method should be a continuous loop, that sends revoke
  // messages to lock holders whenever another client wants the
  // same lock
  do {
      CondLockCacheRSM * lock_item_ptr = NULL;
      revoke_queue_.deq(&lock_item_ptr);
      VERIFY(lock_item_ptr != NULL);
      if (rsm->amiprimary())
      {
        pthread_mutex_lock(&m_lock_mutex_);
        int ret_r, r;
        std::string user_id = lock_item_ptr->wait_revoke_queue_.front();
        lock_protocol::lockid_t lid = lock_item_ptr->lid;
        lock_protocol::xid_t xid = lock_item_ptr->lock_info_[user_id];
        lock_item_ptr->wait_revoke_queue_.pop();
        pthread_mutex_unlock(&m_lock_mutex_);
        ret_r = handle(user_id).safebind()->call(rlock_protocol::revoke, lid, xid, r);
        if (ret_r != rlock_protocol::OK) tprintf("rlock_protocol::revoke failed!!\n");
      }
  } while(true);
}


void
lock_server_cache_rsm::retryer()
{

  // This method should be a continuous loop, waiting for locks
  // to be released and then sending retry messages to those who
  // are waiting for it.
  do {
      CondLockCacheRSM * lock_item_ptr = NULL;

      retry_queue_.deq(&lock_item_ptr);
      VERIFY(lock_item_ptr != NULL);
      if (rsm->amiprimary()){
          pthread_mutex_lock(&m_lock_mutex_);
          while(!lock_item_ptr->wait_retry_queue_.empty()) {
            int ret_r, r;
            std::string user_id = lock_item_ptr->wait_retry_queue_.front();
            lock_protocol::lockid_t lid = lock_item_ptr->lid;
            lock_protocol::xid_t xid = lock_item_ptr->lock_info_[user_id];
            lock_item_ptr->wait_retry_queue_.pop();
            pthread_mutex_unlock(&m_lock_mutex_);
            ret_r = handle(user_id).safebind()->call(rlock_protocol::retry, lid, xid, r);
            if (ret_r != rlock_protocol::OK) tprintf("rlock_protocol::retry failed!!\n");
            pthread_mutex_lock(&m_lock_mutex_);
          }
          pthread_mutex_unlock(&m_lock_mutex_);
      }

  } while(true);
}


int lock_server_cache_rsm::acquire(lock_protocol::lockid_t lid, std::string id, 
             lock_protocol::xid_t xid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  pthread_mutex_lock(&m_lock_mutex_);
  CondLockCacheRSM& lock_item =  map_lock_[lid];
    tprintf("lock item lid:%lld, id:%s acquire, user_id:%s, is_lock %d, lock xid=%lld, send xid=%lld, primary:%s\n",
            lid, id.c_str(), lock_item.user_id.c_str(), lock_item.is_locked,
            lock_item.lock_info_[id], xid, rsm->get_primary().c_str());
    if (lock_item.lock_info_[id] < xid || lock_item.is_locked == true) {
        if (lock_item.user_id == id) tprintf("same!!\n");
    }
  if (lock_item.lock_info_[id] < xid){
      if (lock_item.is_locked == false){
            lock_item.is_locked = true;
            lock_item.lock_info_[id] = xid;
            lock_item.user_id = id;
            lock_item.lid = lid;
            r = AcqRet::OK;
        } else {
            lock_item.wait_retry_queue_.push(id);
            lock_item.wait_revoke_queue_.push(lock_item.user_id);
            r = AcqRet::RETRY;
            revoke_queue_.enq(&lock_item);
        }
  } else {
      ret = lock_protocol::RPCERR;
      r = AcqRet::RETRY;
  }
  pthread_mutex_unlock(&m_lock_mutex_);
  return ret;
}

int 
lock_server_cache_rsm::release(lock_protocol::lockid_t lid, std::string id, 
         lock_protocol::xid_t xid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  tprintf("%s send %llu to server\n", id.c_str(), lid);
  pthread_mutex_lock(&m_lock_mutex_);
  if (map_lock_.count(lid) <= 0){
      tprintf("lid %lld is not acquired by anyone, why do you release?\n", lid);
      return ret;
  }
  CondLockCacheRSM& lock_item = map_lock_[lid];
    tprintf("%s send %llu to server, lock_item xid=%lld, send xid=%lld, is_lock:%d\n",
            id.c_str(), lid, lock_item.lock_info_[id], xid, lock_item.is_locked);
  if (lock_item.lock_info_[id] <= xid && lock_item.is_locked == true){
      if (lock_item.user_id == id) lock_item.is_locked = false;
      retry_queue_.enq(&lock_item);
  } else {
      tprintf(" error lock_item xid=%lld, send xid=%lld\n", lock_item.lock_info_[id], xid);
  }
  pthread_mutex_unlock(&m_lock_mutex_);
  return ret;
}

std::string
lock_server_cache_rsm::marshal_state()
{
  marshall rep;
  ScopeLock ml(&m_lock_mutex_);

  rep << (unsigned int)map_lock_.size();
  foreach(map_lock_, lock_it){
      rep << lock_it->first;
      rep << lock_it->second.is_locked;
      rep << lock_it->second.lid;
      //rep << map_it->second.pcond; pcond is not used after lab4
      rep << lock_it->second.user_id;

      rep << (unsigned int)lock_it->second.lock_info_.size();
      foreach(lock_it->second.lock_info_, info_it){
          rep << info_it->first;
          rep << info_it->second;
      }

      std::queue<std::string> tmp_queue = lock_it->second.wait_retry_queue_;
      rep << (unsigned int)tmp_queue.size();
      while(!tmp_queue.empty()){
          rep << tmp_queue.front();
          tmp_queue.pop();
      }

      tmp_queue = lock_it->second.wait_revoke_queue_;
      rep << (unsigned int)tmp_queue.size();
        while(!tmp_queue.empty()){
        rep << tmp_queue.front();
        tmp_queue.pop();
      }
  }

  return rep.str();
}

void
lock_server_cache_rsm::unmarshal_state(std::string state)
{
    unmarshall rep(state);
    ScopeLock ml(&m_lock_mutex_);
    map_lock_.clear();
    unsigned int lock_size = 0;
    rep >> lock_size;
    for(unsigned int i = 0; i < lock_size; i++){
        lock_protocol::lockid_t lockid = 0;
        rep >> lockid;
        CondLockCacheRSM& lock_item = map_lock_[lockid];
        rep >> lock_item.is_locked;
        rep >> lock_item.lid;
        rep >> lock_item.user_id;

        unsigned int info_size = 0;
        rep >> info_size;
        std::map<std::string, lock_protocol::xid_t>& info_item = lock_item.lock_info_;
        info_item.clear();
        for(unsigned int j = 0; j < info_size; j++){
            std::string key;
            lock_protocol::xid_t value;
            rep >> key;
            rep >> value;
            info_item[key] = value;
        }

        unsigned int retry_size = 0;
        rep >> retry_size;
        std::queue<std::string>& retry_queue = lock_item.wait_retry_queue_;
        {
            //clear the queue
            std::queue<std::string> empty_queue;
            std::swap(retry_queue, empty_queue);
        }
        for(unsigned int k = 0; k < retry_size; k++){
            std::string item;
            rep >> item;
            retry_queue.push(item);
        }

        unsigned int revoke_size = 0;
        rep >> revoke_size;
        std::queue<std::string>& revoke_queue = lock_item.wait_revoke_queue_;
        {
            //clear the queue
            std::queue<std::string> empty_queue;
            std::swap(revoke_queue, empty_queue);
        }
        for(unsigned int l = 0; l < revoke_size; l++){
            std::string item;
            rep >> item;
            revoke_queue.push(item);
        }
    }

}

lock_protocol::status
lock_server_cache_rsm::stat(lock_protocol::lockid_t lid, int &r)
{
  printf("stat request\n");
  r = nacquire;
  return lock_protocol::OK;
}

