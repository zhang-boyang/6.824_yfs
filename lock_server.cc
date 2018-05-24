// the lock server implementation

#include "lock_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

lock_server::lock_server():
  nacquire (0)
{
  pthread_mutex_init(&pcontrol_.pmutex, NULL);
}

lock_server::~lock_server() {
	pthread_mutex_destroy(&pcontrol_.pmutex);
}

lock_protocol::status
lock_server::stat(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  printf("stat request from clt %d\n", clt);
  r = nacquire;
  return ret;
}

lock_protocol::status
lock_server::acquire_lock(int clt, lock_protocol::lockid_t lid, int &r)
{
	lock_protocol::status ret = lock_protocol::OK;
	pthread_mutex_lock(&pcontrol_.pmutex);
	if (lock_map_.count(lid) <= 0) {
		lock_map_[lid].is_locked = true;
	} else {
		while (lock_map_[lid].is_locked == true) {
			pthread_cond_wait(&lock_map_[lid].pcond, &pcontrol_.pmutex);
		}
		lock_map_[lid].is_locked = true;
	}
	ret = lock_protocol::OK;
	pthread_mutex_unlock(&pcontrol_.pmutex);
	return ret;
}

lock_protocol::status
lock_server::release_lock(int clt, lock_protocol::lockid_t lid, int &r) {
	lock_protocol::status ret = lock_protocol::OK;
	pthread_mutex_lock(&pcontrol_.pmutex);
	if (lock_map_.count(lid) <= 0){
		pthread_mutex_unlock(&pcontrol_.pmutex);
		ret = lock_protocol::IOERR;
	}
	if (lock_map_[lid].is_locked) {
		lock_map_[lid].is_locked = false;
		pthread_cond_signal(&lock_map_[lid].pcond);
	} else {
		std::cout << "lid:" << lid << "is not locked, why release?" << std::endl;
	}
	pthread_mutex_unlock(&pcontrol_.pmutex);
	return ret;
}
