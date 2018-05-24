// the extent server implementation

#include "extent_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctime>
#include <sys/time.h>


extent_server::extent_server()
{
	pthread_mutex_init(&pmutex_file_block_map_, NULL);
	int ret;
	//create for root
	put(1,"", ret);
}

extent_server::~extent_server()
{
	pthread_mutex_destroy(&pmutex_file_block_map_);
}

class extent_lock{
	pthread_mutex_t * m;
  public:
	extent_lock(pthread_mutex_t * mutex):
		m(mutex)
  	{
		pthread_mutex_lock(m);
	}
	~extent_lock(){
		pthread_mutex_unlock(m);
	}
};

int extent_server::put(extent_protocol::extentid_t id, std::string buf, int &r)
{
  // You fill this in for Lab 2.
//	extent_lock ex_lc(&pmutex_file_block_map_);
  time_t now = time(0);

  if (file_block_.count(id) <= 0){
	  file_block_[id].first.ctime = static_cast<int>(now);
	  file_block_[id].first.mtime = static_cast<int>(now);
	  file_block_[id].first.atime = static_cast<int>(now);
  } else {
	  file_block_[id].first.mtime = static_cast<int>(now);
	  file_block_[id].first.ctime = static_cast<int>(now);
  }
  file_block_[id].first.size = buf.size();
  file_block_[id].second = buf;

  r = 0;
  return extent_protocol::OK;
}

int extent_server::get(extent_protocol::extentid_t id, std::string &buf)
{
  // You fill this in for Lab 2.
//	extent_lock ex_lc(&pmutex_file_block_map_);
  time_t now = time(0);
  if (file_block_.count(id) <= 0){
	  return extent_protocol::NOENT;
  }
  file_block_[id].first.atime = static_cast<int>(now);
  buf = file_block_[id].second;
  return extent_protocol::OK;
}

int extent_server::getattr(extent_protocol::extentid_t id, extent_protocol::attr &a)
{
  // You fill this in for Lab 2.
  // You replace this with a real implementation. We send a phony response
  // for now because it's difficult to get FUSE to do anything (including
  // unmount) if getattr fails.
//	extent_lock ex_lc(&pmutex_file_block_map_);
	if (file_block_.count(id) <= 0){
	  return extent_protocol::NOENT;
	}
	a = file_block_[id].first;
	return extent_protocol::OK;
}

int extent_server::remove(extent_protocol::extentid_t id, int &r)
{
  // You fill this in for Lab 2.
//	extent_lock ex_lc(&pmutex_file_block_map_);
	std::map<uint64_t, FileContent>::iterator mit = file_block_.find(id);
	if (mit == file_block_.end()){
		  r = 0;
		  return extent_protocol::NOENT;
	}
	file_block_.erase(mit);
	r = 0;
	return extent_protocol::OK;
}
int extent_server::GetInodeNum(int is_file, extent_protocol::extentid_t& rino)
{
	extent_lock ex_lc(&pmutex_file_block_map_);
	extent_protocol::extentid_t ino;
	struct timeval tv;
	gettimeofday(&tv,NULL);
	srand(tv.tv_usec);
	do{
		ino = rand() % 0x80000000LLU;
		if (ino == 1)
			continue;
		if(is_file){
			ino |= 0x80000000LLU;
		}
	}while(file_block_.count(ino) > 0);
	rino = ino;
	return extent_protocol::OK;
}

