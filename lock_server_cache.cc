// the caching lock server implementation

#include "lock_server_cache.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "lang/verify.h"
#include "handle.h"
#include "tprintf.h"

lock_server_cache::lock_server_cache()
{
    nacquire = 0;
    pthread_mutex_init(&m_lock_mutex_, NULL);
    pthread_mutex_init(&m_retry_mutex_, NULL);
}

lock_server_cache::~lock_server_cache()
{
	pthread_mutex_destroy(&m_lock_mutex_);
	pthread_mutex_destroy(&m_retry_mutex_);
}
int lock_server_cache::acquire(lock_protocol::lockid_t lid, std::string id, 
                               int &r)
{
      AcqRet::status ret;
	  ScopeLock map_lock(&m_lock_mutex_);
	  CondLockCache& lock_item =  m_map_lock_[lid];
	  if (lock_item.is_locked == false){
		  lock_item.is_locked = true;
		  lock_item.user_id_ = id;
		  ret = AcqRet::OK;
	  }else{
		  VERIFY (!lock_item.user_id_.empty());
		  pthread_mutex_lock(&m_retry_mutex_);
		  m_retry_queue_[lid].push(id);
		  pthread_mutex_unlock(&m_retry_mutex_);
		  //ConnectToClient conn_c(lock_item.user_id_);
		  ret = AcqRet::RETRY;
		  pthread_mutex_unlock(&m_lock_mutex_);
		  int ret_r;
		  ret_r = handle(lock_item.user_id_).safebind()->call(rlock_protocol::revoke, lid, r);
		  if (ret_r != rlock_protocol::OK) tprintf("rlock_protocol::revoke failed!!\n");
	  }
  return ret;
}

lock_protocol::status
lock_server_cache::release(lock_protocol::lockid_t lid, std::string id, 
         int &r)
{
  tprintf("%s send %llu to server\n", id.c_str(), lid);
  lock_protocol::status ret = lock_protocol::OK;
  ScopeLock map_lock(&m_lock_mutex_);
  if (m_map_lock_.count(lid) <= 0){
	  tprintf("lid is not acquired by anyone, why do you release?\n");
  }
  CondLockCache& lock_item =  m_map_lock_[lid];
  lock_item.is_locked = false;
  ScopeLock retry_lock(&m_retry_mutex_);
  std::queue<std::string> &rty_q = m_retry_queue_[lid];
  tprintf("now the retry queue size is %zu\n", rty_q.size());
  if (!rty_q.empty()){
      int ret_r;
	  std::string user_id = rty_q.front();
	  rty_q.pop();
	  //ConnectToClient conn_c(user_id);
	  pthread_mutex_unlock(&m_lock_mutex_);
	  pthread_mutex_unlock(&m_retry_mutex_);
	  ret_r = handle(user_id).safebind()->call(rlock_protocol::retry, lid, r);
	  if (ret_r != rlock_protocol::OK) tprintf("rlock_protocol::retry failed!!\n");
	  pthread_mutex_lock(&m_lock_mutex_);
	  pthread_mutex_lock(&m_retry_mutex_);
	  if(!rty_q.empty()){
	    pthread_mutex_unlock(&m_lock_mutex_);
        pthread_mutex_unlock(&m_retry_mutex_);
        ret_r = handle(user_id).safebind()->call(rlock_protocol::revoke, lid, r);
        if (ret_r != rlock_protocol::OK) tprintf("rlock_protocol::revoke failed!!\n");
        pthread_mutex_lock(&m_lock_mutex_);
        pthread_mutex_lock(&m_retry_mutex_);
	  }
  }

  return ret;
}

lock_protocol::status
lock_server_cache::stat(lock_protocol::lockid_t lid, int &r)
{
  tprintf("stat request\n");
  r = nacquire;
  return lock_protocol::OK;
}


ConnectToClient::ConnectToClient(std::string user_id){
	  sockaddr_in dstsock;
	  id = user_id;
	  make_sockaddr(user_id.c_str(), &dstsock);
	  cl = new rpcc(dstsock);
	  if (cl->bind() < 0) {
		printf("lock_client: call bind\n");
	  }
}

lock_protocol::status ConnectToClient::Retry(lock_protocol::lockid_t lid){
	int r;
	lock_protocol::status ret;
	ret = cl->call(rlock_protocol::retry, lid, r);
	VERIFY (ret == lock_protocol::OK);
	return r;
}

lock_protocol::status ConnectToClient::Revoke(lock_protocol::lockid_t lid){
	int r;
	lock_protocol::status ret;
	ret = cl->call(rlock_protocol::revoke, lid, r);
	VERIFY (ret == lock_protocol::OK);
	return r;
}

ConnectToClient::~ConnectToClient(){
	delete cl;
}

