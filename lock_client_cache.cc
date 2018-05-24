// RPC stubs for clients to talk to lock_server, and cache the locks
// see lock_client.cache.h for protocol details.

#include "lock_client_cache.h"
#include "rpc.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include "tprintf.h"


lock_client_cache::lock_client_cache(std::string xdst, 
				     class lock_release_user *_lu)
  : lock_client(xdst), lu(_lu)
{
  rpcs *rlsrpc = new rpcs(0);
  rlsrpc->reg(rlock_protocol::revoke, this, &lock_client_cache::revoke_handler);
  rlsrpc->reg(rlock_protocol::retry, this, &lock_client_cache::retry_handler);

  rlock_port = 0;

  const char *hname;
  hname = "127.0.0.1";
  std::ostringstream host;
  host << hname << ":" << rlsrpc->port();
  id = host.str();
  pthread_mutex_init(&lock_map_mutex_, NULL);
}

lock_client_cache::~lock_client_cache()
{
    for(typeof(m_lock_map_.begin()) mit = m_lock_map_.begin(); mit != m_lock_map_.end(); mit++){
        if (mit->second.status != AcqRet::NONE){
            release(mit->first);
        }
    }
	pthread_mutex_destroy(&lock_map_mutex_);
}

void lock_client_cache::DealRelease(ClientLock & lock_item, lock_protocol::lockid_t lid)
{
    if (lock_item.status == AcqRet::NONE) return;
    if (lock_item.is_revoked){
        int r;
        lock_item.status = AcqRet::NONE;
        lock_item.is_finished = false;
        lock_item.is_revoked = false;
        pthread_mutex_unlock(&lock_map_mutex_);
        lu->dorelease(lid);
        lock_protocol::status ret = cl->call(lock_protocol::release, lid, id, r);
        tprintf("I send it %llu back to server\n", lid);
        VERIFY(ret == lock_protocol::OK);
        pthread_mutex_lock(&lock_map_mutex_);
    }else{
        lock_item.status = AcqRet::FREE;
        lock_item.is_finished = true;
    }
    pthread_cond_broadcast(&lock_item.wait_free_);
    pthread_cond_broadcast(&lock_item.wait_acq_);
}
void lock_client_cache::DealWithStatusACQ(ClientLock & lock_item, lock_protocol::lockid_t lid)
{
    if (lock_item.is_retried){
        SendAcqToSvr(lock_item, lid);
        tprintf("I am here retried 1\n");
    }
    while(lock_item.status == AcqRet::ACQING && !lock_item.is_retried){
        tprintf("I am waiting here\n");
        pthread_cond_wait(&lock_item.wait_acq_, &lock_map_mutex_);
    }
    if (lock_item.is_retried && lock_item.status == AcqRet::ACQING){
        SendAcqToSvr(lock_item, lid);
        tprintf("I am here retried 2\n");
    }
}
void lock_client_cache::DealWithStatusFREE(ClientLock & lock_item, lock_protocol::lockid_t lid)
{
    lock_item.is_finished = false;
     lock_item.status = AcqRet::LOCK;
}
void lock_client_cache::DealWithStatusLOCK(ClientLock & lock_item, lock_protocol::lockid_t lid)
{
    while(lock_item.status == AcqRet::LOCK){
        pthread_cond_wait(&lock_item.wait_free_, &lock_map_mutex_);
    }
}
void lock_client_cache::DealWithStatusNONE(ClientLock & lock_item, lock_protocol::lockid_t lid)
{

    SendAcqToSvr(lock_item, lid);
}

int lock_client_cache::SendAcqToSvr(ClientLock & lock_item, lock_protocol::lockid_t lid){
    int r;
    lock_item.is_retried  = false;
    pthread_mutex_unlock(&lock_map_mutex_);
    int ret = cl->call(lock_protocol::acquire, lid, id, r);
    pthread_mutex_lock(&lock_map_mutex_);
    tprintf("I am send a msg to svr, ret=%d\n", ret);
    if (ret == AcqRet::OK){
        lock_item.status = AcqRet::FREE;
    } else if (ret) {
        lock_item.status = AcqRet::ACQING;
    } else {
        tprintf("unknow svr return %d\n", ret);
    }
    return ret;

}

lock_protocol::status
lock_client_cache::acquire(lock_protocol::lockid_t lid)
{
  int ret = lock_protocol::OK;
  ScopeLock map_lock(&lock_map_mutex_);
  ClientLock &lock_item = m_lock_map_[lid];
  bool  is_used = false;
  while(!is_used){
    tprintf("lock_item.status is %d\n", lock_item.status);
	switch(lock_item.status)
	{
	  case AcqRet::NONE:
	      DealWithStatusNONE(lock_item, lid);
		  break;
	  case AcqRet::FREE:
	      DealWithStatusFREE(lock_item, lid);
	      is_used = true;
		  break;
	  case AcqRet::LOCK:
	      DealWithStatusLOCK(lock_item, lid);
		  break;
	  case AcqRet::ACQING:
	      DealWithStatusACQ(lock_item, lid);
		  break;
	  default:
		VERIFY(false);
	}
  }
  return ret;
}

lock_protocol::status
lock_client_cache::release(lock_protocol::lockid_t lid)
{

  ScopeLock map_lock(&lock_map_mutex_);
  ClientLock &lock_item = m_lock_map_[lid];
  DealRelease(lock_item, lid);

  return lock_protocol::OK;

}

rlock_protocol::status
lock_client_cache::revoke_handler(lock_protocol::lockid_t lid, 
                                  int &)
{
  int ret = rlock_protocol::OK;
  ScopeLock map_lock(&lock_map_mutex_);
  ClientLock &lock_item = m_lock_map_[lid];
  lock_item.is_revoked = true;
  if (lock_item.is_finished){
      tprintf("I release from here\n");
      DealRelease(lock_item, lid);
  }

  return ret;
}

rlock_protocol::status
lock_client_cache::retry_handler(lock_protocol::lockid_t lid, 
                                 int &)
{
  int ret = rlock_protocol::OK;
  ScopeLock map_lock(&lock_map_mutex_);
  ClientLock &lock_item = m_lock_map_[lid];
  lock_item.is_retried = true;
  pthread_cond_signal(&lock_item.wait_acq_);
  pthread_cond_signal(&lock_item.wait_free_);
  tprintf("retry_handler: %llu\n", lid);
  return ret;
}



