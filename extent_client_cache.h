/*
 * extent_client_cache.h
 *
 *  Created on: 2017年12月25日
 *      Author: zhangby
 */

#ifndef EXTENT_CLIENT_CACHE_H_
#define EXTENT_CLIENT_CACHE_H_

#include "extent_client.h"
class FileData {
 public:
    FileContent file_content_;
    bool is_present;
    bool is_dirty;

    FileData():
        is_present(false),
        is_dirty(false)
    {}
};

class extent_client_cache : public extent_client {
 private:
  std::map<uint64_t, FileData> extent_file_cache_;
 public:
  extent_client_cache(std::string dst):
      extent_client(dst){};

  extent_protocol::status get(extent_protocol::extentid_t eid,
                  std::string &buf);
  extent_protocol::status getattr(extent_protocol::extentid_t eid,
                  extent_protocol::attr &a);
  extent_protocol::status put(extent_protocol::extentid_t eid, std::string buf);
  extent_protocol::status remove(extent_protocol::extentid_t eid);
  extent_protocol::status flush(extent_protocol::extentid_t eid);
};

#endif /* EXTENT_CLIENT_CACHE_H_ */
