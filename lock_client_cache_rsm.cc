// RPC stubs for clients to talk to lock_server, and cache the locks
// see lock_client.cache.h for protocol details.

#include "lock_client_cache_rsm.h"
#include "rpc.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include "tprintf.h"

#include "rsm_client.h"
#define WAIT_TIMES (3)
static void *
releasethread(void *x)
{
  lock_client_cache_rsm *cc = (lock_client_cache_rsm *) x;
  cc->releaser();
  return 0;
}

int lock_client_cache_rsm::last_port = 0;

lock_client_cache_rsm::lock_client_cache_rsm(std::string xdst, 
				     class lock_release_user *_lu)
  : lock_client(xdst), lu(_lu)
{
  srand(time(NULL)^last_port);
  rlock_port = ((rand()%32000) | (0x1 << 10));
  const char *hname;
  // VERIFY(gethostname(hname, 100) == 0);
  hname = "127.0.0.1";
  std::ostringstream host;
  host << hname << ":" << rlock_port;
  id = host.str();
  last_port = rlock_port;
  rpcs *rlsrpc = new rpcs(rlock_port);
  rlsrpc->reg(rlock_protocol::revoke, this, &lock_client_cache_rsm::revoke_handler);
  rlsrpc->reg(rlock_protocol::retry, this, &lock_client_cache_rsm::retry_handler);
  xid = 0;
  // You fill this in Step Two, Lab 7
  // - Create rsmc, and use the object to do RPC 
  //   calls instead of the rpcc object of lock_client
  rsmc = new rsm_client(xdst);
  VERIFY (rsmc != NULL);
  pthread_t th;
  int r = pthread_create(&th, NULL, &releasethread, (void *) this);
  VERIFY (r == 0);
  pthread_mutex_init(&lock_map_mutex_, NULL);
}

lock_client_cache_rsm::~lock_client_cache_rsm()
{
    pthread_mutex_destroy(&lock_map_mutex_);
}

void lock_client_cache_rsm::DealRelease(ClientLockRSM & lock_item, lock_protocol::lockid_t lid)
{
    if (lock_item.status == AcqRet::NONE) {
        //primary done, req will resend, so this situation will happend
        //release_queue_.enq(&lock_item);
        return;
    }
    if (lock_item.is_revoked){
        lock_item.status = AcqRet::RELEASING;
        release_queue_.enq(&lock_item);
    }else{
        lock_item.status = AcqRet::FREE;
        lock_item.is_finished = true;
    }
    pthread_cond_broadcast(&lock_item.wait_free_);
    pthread_cond_broadcast(&lock_item.wait_acq_);
}
void lock_client_cache_rsm::DealWithStatusACQ(ClientLockRSM & lock_item, lock_protocol::lockid_t lid)
{
    if (lock_item.is_retried){
        SendAcqToSvr(lock_item, lid);
        tprintf("I am here retried !\n");
    }
    while(lock_item.status == AcqRet::ACQING && !lock_item.is_retried){
        tprintf("I am waiting here\n");
        struct timeval tp;
        struct timespec ts;
        int ret;
        gettimeofday(&tp, NULL);
        ts.tv_sec = tp.tv_sec + WAIT_TIMES;
        ts.tv_nsec = tp.tv_usec * 1000;
        ret = pthread_cond_timedwait(&lock_item.wait_acq_, &lock_map_mutex_, &ts);
        if (ret == ETIMEDOUT) lock_item.is_retried = true;
    }
    if (lock_item.is_retried && lock_item.status == AcqRet::ACQING){
        SendAcqToSvr(lock_item, lid);
        tprintf("I am here retried 2\n");
    }
}
void lock_client_cache_rsm::DealWithStatusFREE(ClientLockRSM & lock_item, lock_protocol::lockid_t lid)
{
    lock_item.is_finished = false;
     lock_item.status = AcqRet::LOCK;
}
void lock_client_cache_rsm::DealWithStatusLOCK(ClientLockRSM & lock_item, lock_protocol::lockid_t lid)
{
    while(lock_item.status == AcqRet::LOCK){
        pthread_cond_wait(&lock_item.wait_free_, &lock_map_mutex_);
    }
}
void lock_client_cache_rsm::DealWithStatusNONE(ClientLockRSM & lock_item, lock_protocol::lockid_t lid)
{

    SendAcqToSvr(lock_item, lid);
}

int lock_client_cache_rsm::SendAcqToSvr(ClientLockRSM & lock_item, lock_protocol::lockid_t lid){
    int r;
    xid++;
    lock_item.is_retried  = false;
    lock_item.xid = xid;
    pthread_mutex_unlock(&lock_map_mutex_);
    int ret = rsmc->call(lock_protocol::acquire, lid, id, xid ,r);
    pthread_mutex_lock(&lock_map_mutex_);
    tprintf("I sent a msg to svr, ret=%d, xid=%lld, ret=%d\n", r, xid, ret);
    if(ret != lock_protocol::OK) return ret;
    if (r == AcqRet::OK){
        lock_item.status = AcqRet::FREE;
        tprintf("%s said OK lid:%lld\n", rsmc->primary.c_str(), lid);
    } else if (r == AcqRet::RETRY) {
        lock_item.status = AcqRet::ACQING;
    } else {
        tprintf("unknow svr return %d\n", ret);
    }
    return ret;

}


void
lock_client_cache_rsm::releaser()
{

  // This method should be a continuous loop, waiting to be notified of
  // freed locks that have been revoked by the server, so that it can
  // send a release RPC.
  do{
      ClientLockRSM * lock_item_ptr = NULL;
      release_queue_.deq(&lock_item_ptr);
      VERIFY(lock_item_ptr != NULL);
      pthread_mutex_lock(&lock_map_mutex_);
      lock_protocol::lockid_t lid = lock_item_ptr->lid;
      lock_protocol::xid_t xid_ = lock_item_ptr->xid;
      if (lu != NULL) lu->dorelease(lid);
      int r;
      pthread_mutex_unlock(&lock_map_mutex_);
      lock_protocol::status ret = rsmc->call(lock_protocol::release, lid, id, xid_, r);
      tprintf("I send it %llu back to server\n", lid);
      VERIFY(ret == lock_protocol::OK);
      pthread_mutex_lock(&lock_map_mutex_);
      lock_item_ptr->is_finished = false;
      lock_item_ptr->is_revoked = false;
      lock_item_ptr->status = AcqRet::NONE;
      pthread_cond_signal(&lock_item_ptr->wait_free_);
      pthread_mutex_unlock(&lock_map_mutex_);
  }while(true);

}


lock_protocol::status
lock_client_cache_rsm::acquire(lock_protocol::lockid_t lid)
{
  int ret = lock_protocol::OK;
  pthread_mutex_lock(&lock_map_mutex_);
  ClientLockRSM &lock_item = map_lock_[lid];
  lock_item.lid = lid;
  bool  is_used = false;
  while(!is_used){
  tprintf("lock_item.status is %d\n", lock_item.status);
  switch(lock_item.status){
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
    case AcqRet::RELEASING:
        pthread_cond_wait(&lock_item.wait_free_, &lock_map_mutex_);
        break;
    default:
      VERIFY(false);
      }
    }
  pthread_mutex_unlock(&lock_map_mutex_);
  return ret;
}

lock_protocol::status
lock_client_cache_rsm::release(lock_protocol::lockid_t lid)
{
  ScopeLock map_lock(&lock_map_mutex_);
  ClientLockRSM &lock_item = map_lock_[lid];
  DealRelease(lock_item, lid);
  return lock_protocol::OK;

}


rlock_protocol::status
lock_client_cache_rsm::revoke_handler(lock_protocol::lockid_t lid, 
			          lock_protocol::xid_t xid, int &r)
{
  int ret = rlock_protocol::OK;
  ScopeLock map_lock(&lock_map_mutex_);
  tprintf("revoke_handler lid %lld\n", lid);
  ClientLockRSM &lock_item = map_lock_[lid];
  if (lock_item.xid == xid){
    lock_item.is_revoked = true;
    if (lock_item.is_finished){
        tprintf("I release from here\n");
        DealRelease(lock_item, lid);
    }
  }else if (lock_item.xid > xid){
      if (lock_item.status == AcqRet::ACQING || lock_item.status == AcqRet::NONE){
          release_queue_.enq(&lock_item);
      } else {
          lock_item.is_revoked = true;
      }
  } else {
      tprintf("revoke error: lock_item.xid=%lld, xid=%lld\n", lock_item.xid, xid);
  }

  return ret;
}

rlock_protocol::status
lock_client_cache_rsm::retry_handler(lock_protocol::lockid_t lid, 
			         lock_protocol::xid_t xid, int &r)
{
  int ret = rlock_protocol::OK;
  ScopeLock map_lock(&lock_map_mutex_);
  ClientLockRSM &lock_item = map_lock_[lid];
  if (lock_item.xid <= xid){
      lock_item.is_retried = true;
      pthread_cond_signal(&lock_item.wait_acq_);
      pthread_cond_signal(&lock_item.wait_free_);
      tprintf("retry_handler: %llu\n", lid);
  }

  return ret;
}


