// this is the extent server

#ifndef extent_server_h
#define extent_server_h

#include <string>
#include <map>
#include "extent_protocol.h"



typedef std::pair<extent_protocol::attr, std::string> FileContent;
class extent_server {

 public:
  std::map<uint64_t, FileContent> file_block_;
  pthread_mutex_t pmutex_file_block_map_;
  extent_server();
  ~extent_server();

  int put(extent_protocol::extentid_t id, std::string, int &);
  int get(extent_protocol::extentid_t id, std::string &);
  int getattr(extent_protocol::extentid_t id, extent_protocol::attr &);
  int remove(extent_protocol::extentid_t id, int &);
  int GetInodeNum(int is_file, extent_protocol::extentid_t& rino);
};

#endif 







