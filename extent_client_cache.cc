/*
 * extent_client_cache.cc
 *
 *  Created on: 2017年12月25日
 *      Author: zhangby
 */
// RPC stubs for clients to talk to extent_server

#include "extent_client_cache.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include "tprintf.h"
extent_protocol::status
extent_client_cache::get(extent_protocol::extentid_t eid, std::string &buf)
{
  extent_protocol::status ret = extent_protocol::OK;
  if (extent_file_cache_[eid].is_present){
      time_t now = time(0);
      extent_file_cache_[eid].file_content_.first.atime = static_cast<int>(now);
      buf = extent_file_cache_[eid].file_content_.second;
  }else{
    extent_protocol::attr &attr = extent_file_cache_[eid].file_content_.first;
    ret = cl->call(extent_protocol::get, eid, buf);
    ret = cl->call(extent_protocol::getattr, eid, attr);
    extent_file_cache_[eid].is_present = true;
    extent_file_cache_[eid].file_content_.second = buf;
  }
  return ret;
}

extent_protocol::status
extent_client_cache::getattr(extent_protocol::extentid_t eid,
               extent_protocol::attr &attr)
{
  extent_protocol::status ret = extent_protocol::OK;
  if (extent_file_cache_[eid].is_present){
      attr = extent_file_cache_[eid].file_content_.first;
  } else {
      ret = cl->call(extent_protocol::getattr, eid, attr);
  }
  return ret;
}

extent_protocol::status
extent_client_cache::put(extent_protocol::extentid_t eid, std::string buf)
{
  extent_protocol::status ret = extent_protocol::OK;
  time_t now = time(0);

  std::map<uint64_t, FileData>::iterator file_cache_it = extent_file_cache_.find(eid);
  if (file_cache_it == extent_file_cache_.end()){
      extent_file_cache_[eid].file_content_.first.atime = static_cast<int>(now);
  }
  extent_file_cache_[eid].file_content_.first.mtime = static_cast<int>(now);
  extent_file_cache_[eid].file_content_.first.ctime = static_cast<int>(now);
  //int r;
  //ret = cl->call(extent_protocol::put, eid, buf, r);
  extent_file_cache_[eid].file_content_.first.size = buf.size();
  extent_file_cache_[eid].file_content_.second = buf;
  extent_file_cache_[eid].is_dirty = true;
  extent_file_cache_[eid].is_present = true;
  return ret;
}

extent_protocol::status
extent_client_cache::remove(extent_protocol::extentid_t eid)
{
  extent_protocol::status ret = extent_protocol::OK;
  std::map<uint64_t, FileData>::iterator file_cache_it = extent_file_cache_.find(eid);
  if (file_cache_it != extent_file_cache_.end()){
      extent_file_cache_.erase(file_cache_it);
  }else{
      int r;
      ret = cl->call(extent_protocol::remove, eid, r);
  }

  return ret;
}

extent_protocol::status extent_client_cache::flush(extent_protocol::extentid_t eid)
{
    extent_protocol::status ret = extent_protocol::OK;
    std::map<uint64_t, FileData>::iterator file_cache_it = extent_file_cache_.find(eid);
    if (file_cache_it == extent_file_cache_.end()){
        int r;
        ret = cl->call(extent_protocol::remove, eid, r);
        return ret;
    }

    if (file_cache_it->second.is_dirty){
        int r;
        std::string &buf = file_cache_it->second.file_content_.second;
        ret = cl->call(extent_protocol::put, eid, buf, r);
        tprintf("put %d back to server, ret=%d\n", eid, ret);
    }
    extent_file_cache_[eid].is_dirty   = false;
    extent_file_cache_[eid].is_present = false;
    return ret;
}



