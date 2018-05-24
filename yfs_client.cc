// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include "lock_client_cache.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cstring>
#define MAX_INODE_NUM (16384) 	//(1024*1024/64)
static uint64_t inode_bit_block_map_[MAX_INODE_NUM];
static uint64_t inode_bit_dir_map_[MAX_INODE_NUM];

static int32_t FindZeroBitPos(const uint64_t bit_map)
{
	int32_t pos = 0;
	uint64_t bit_map_64 = bit_map;
	if ((bit_map_64 & 0xffffffffLLU) == 0xffffffffLLU) {
		bit_map_64 >>= 32;
		pos += 32;
	}

	if ((bit_map_64 & 0x0000ffffLLU) == 0x0000ffffLLU) {
		bit_map_64 >>= 16;
		pos += 16;
	}

	if ((bit_map_64 & 0x000000ffLLU) == 0x000000ffLLU) {
		bit_map_64 >>= 8;
		pos += 8;
	}

	if ((bit_map_64 & 0x0000000fLLU) == 0x0000000fLLU) {
		bit_map_64 >>= 4;
		pos += 4;
	}

	if ((bit_map_64 & 0x00000003LLU) == 0x00000003LLU) {
		bit_map_64 >>= 2;
		pos += 2;
	}
	if ((bit_map_64 & 0x00000001LLU) == 0x00000001LLU) {
		bit_map_64 >>= 1;
		pos += 1;
	}
	return pos;
}


uint64_t yfs_client::FindIdelInodeNum(const uint64_t inode_bit_map_[])
{
	uint64_t idel_inum = 0;
	int ret_num;
	for(uint32_t i = 0; i < MAX_INODE_NUM; i++) {
		if (inode_bit_map_[i] != 0xffffffffffffffffLLU){
			//printf("inode_bit_map_ %016llX\n", inode_bit_map_[i]);
			ret_num = FindZeroBitPos(inode_bit_map_[i]);
			printf("FindIdelInodeNum ret_num %d\n", ret_num);
			if (ret_num == -1) {
				// can not find an available inum
				return idel_inum;
			} else {
				return (i * 64 + ret_num + 1);
			}
		}
	}
	return idel_inum;
}

int yfs_client::SetInodeBitMap(uint64_t inum, uint64_t inode_bit_map_[]) {
	int ret = -1;
	if (inum == 0) {
		//inode num 0 is not used
		return ret;
	}
	inum--;
	int section = inum / 64;
	int bit_pos = inum % 64;

	if (inode_bit_map_[section] & (0x1LLU << bit_pos)) {
		ret = -2;
		return ret;
	} else {
		inode_bit_map_[section] |= (0x1LLU << bit_pos);
		ret = 0;
	}
	return ret;
}

int yfs_client::UnSetInodeBitMap(uint64_t inum, uint64_t inode_bit_map_[]) {
	int ret = -1;
	if (inum == 0) {
		return ret;
	}
	inum--;
	int section = inum / 64;
	int bit_pos = inum % 64;
	//printf("before inode_bit_map_ %016llX\n", inode_bit_map_[section]);
	if (inode_bit_map_[section] & (0x1LLU << bit_pos)) {
		inode_bit_map_[section] &= (~(0x1LLU << bit_pos));
		//printf("after inode_bit_map_ %016llX\n", inode_bit_map_[section]);
		ret = 0;
	} else {
		ret = -2;
	}
	return ret;
}
uint64_t yfs_client::GetAvailInumNotUse(bool is_file_type){

	uint64_t * bit_map;
	uint64_t inum = 0;
	pthread_mutex_lock(&mutex_inode_map_);
	if (is_file_type){
		bit_map = inode_bit_block_map_;
	}else{
		bit_map = inode_bit_dir_map_;
	}
	inum = FindIdelInodeNum(bit_map);
	printf("GetAvailInum inum %lu\n", inum);
	if (inum > 0){
		int ret = SetInodeBitMap(inum, bit_map);
		printf("GetAvailInum ret %d\n", ret);
		if (ret == 0){
			pthread_mutex_unlock(&mutex_inode_map_);
			if(is_file_type){
				return (inum | (0x80000000LLU));
			} else {
				return inum;
			}

		}
	}
	pthread_mutex_unlock(&mutex_inode_map_);
	return 0;
}

uint64_t yfs_client::GetAvailInum(bool is_file_type){
	extent_protocol::extentid_t inode = 0;

	if (ec->getinode(inode, (int)is_file_type) != yfs_client::OK){
		return 0;
	}
	return inode;
}

int yfs_client::RestoreInumNotUse(bool is_file_type, uint64_t inum){
	int ret = 0;
	uint64_t * bit_map;
	if (inum == 0 || inum == 0x80000000LLU) {
		return -3;
	}
	pthread_mutex_lock(&mutex_inode_map_);
	if (is_file_type) {
		bit_map = inode_bit_block_map_;
		inum &=(~(0x80000000LLU));
	}else{
		bit_map = inode_bit_dir_map_;
	}
	ret = UnSetInodeBitMap(inum, bit_map);
	pthread_mutex_unlock(&mutex_inode_map_);
	return ret;
}

int yfs_client::RestoreInum(bool is_file_type, uint64_t inum){

	if (is_file_type) {
		inum &=(~(0x80000000LLU));
	}
	ec->remove(inum);
	return 0;
}


yfs_client::yfs_client(std::string extent_dst, std::string lock_dst)
{
  ec = new extent_client(extent_dst);
  //lc = new lock_client(lock_dst);
  ec = new extent_client_cache(extent_dst);
  lu = new lock_release_user_dev(static_cast<extent_client_cache*>(ec));
  lc = new lock_client_cache(lock_dst, lu);
  pthread_mutex_init(&mutex_inode_map_, NULL);
  //1 for root
  inode_bit_block_map_[0] |= 0x1LLU;
  inode_bit_dir_map_[0] |= 0x1LLU;
  this->lock_dst = lock_dst;

}

yfs_client::~yfs_client()
{
	delete ec;
	pthread_mutex_destroy(&mutex_inode_map_);
}

yfs_client::inum
yfs_client::n2i(std::string n)
{
  std::istringstream ist(n);
  unsigned long long finum;
  ist >> finum;
  return finum;
}

std::string
yfs_client::filename(inum inum)
{
  std::ostringstream ost;
  ost << inum;
  return ost.str();
}

bool
yfs_client::isfile(inum inum)
{
  if(inum & 0x80000000)
    return true;
  return false;
}

bool
yfs_client::isdir(inum inum)
{
  return ! isfile(inum);
}

int
yfs_client::getfile(inum inum, fileinfo &fin)
{
  int r = OK;
  // You modify this function for Lab 3
  // - hold and release the file lock
  YfsLock(lc, inum);
  printf("getfile %016llx\n", inum);
  extent_protocol::attr a;
  if (ec->getattr(inum, a) != extent_protocol::OK) {
    r = IOERR;
    goto release;
  }

  fin.atime = a.atime;
  fin.mtime = a.mtime;
  fin.ctime = a.ctime;
  fin.size = a.size;

 release:

  return r;
}

int
yfs_client::getdir(inum inum, dirinfo &din)
{
  int r = OK;
  // You modify this function for Lab 3
  // - hold and release the directory lock
  YfsLock(lc, inum);
  printf("getdir %016llx\n", inum);
  extent_protocol::attr a;
  if (ec->getattr(inum, a) != extent_protocol::OK) {
    r = IOERR;
    goto release;
  }
  din.atime = a.atime;
  din.mtime = a.mtime;
  din.ctime = a.ctime;

 release:
  return r;
}

int yfs_client::getdata(inum ino, std::string &data){
	int r = OK;
	printf("getdata %016llx\n", ino);
	if (ec->get(ino, data) != extent_protocol::OK) {
		r = IOERR;
		goto release;
	}
 release:
 return r;
}

int yfs_client::putdata(inum ino, std::string &data){
	int r = OK;
	printf("putdata %016llx\n", ino);
	if (ec->put(ino, data) != extent_protocol::OK) {
		r = IOERR;
		goto release;
	}
 release:
 return r;
}

yfs_client::status yfs_client::read(uint64_t ino, size_t size, off_t off, std::string &buf){
	  std::string data;
	  YfsLock yl(lc, ino);
	  yfs_client::status ret = getdata(ino, data);
	  if (ret != yfs_client::OK) {
		  return yfs_client::NOENT;
	  }
	  if ((uint64_t)off >= data.size()) {
		  buf = "";
	  }else if ((off + size) > data.size()){
		  buf = data.substr(off);
	  }else{
		  buf = data.substr(off, size);
	  }
	  return yfs_client::OK;
}

yfs_client::status yfs_client::write(uint64_t ino, size_t size, off_t off, const char* buf){
	  std::string data;
	  std::string write_data;
	  YfsLock yl(lc, ino);
	  yfs_client::status ret = getdata(ino, data);
	  if (ret != yfs_client::OK) {
		  return ret;
	  }
	  if ((uint64_t)off > data.size()) {
		  data.resize(off);
	  }
	  write_data = data.substr(0, off);
	  if ((off + size) > data.size()){
		  write_data.append(buf, size);
	  } else {
		  std::string tmp_str(buf, size);
		  write_data += tmp_str;
		  write_data += data.substr(off + size);
	  }
	  ret = putdata(ino, write_data);
	  if (ret != yfs_client::OK){
		  return ret;
	  }
	  return ret;
}

yfs_client::status yfs_client::create(uint64_t parent, const char *name, uint64_t& ret_ino){
	  yfs_client::status ret;
	  std::string data;
	  std::string str_name = name;
	  str_name.resize(MAX_ITEM_NAME_LEN);
	  printf("parent id: %lu\n", parent);
	  printf("file name is %s\n", str_name.c_str());
	  YfsLock yl(lc, parent);
	  ret = getdata(parent, data);
	  if (ret != yfs_client::OK){
		  return yfs_client::NOENT;
	  }
	  DirForm dir_info;
	  dir_info.StringToDirForm(data);
	  if (dir_info.dir_block_.count(str_name) > 0) {
		  return yfs_client::EXIST;
	  }
	  ret_ino = GetAvailInum(true);
	  printf("fuseserver_createhelper ino:%lu\n", ret_ino);
	  if (ret_ino == 0) {
		  return yfs_client::NOENT;
	  }
	  dir_info.dir_block_[str_name] = ret_ino;
	  dir_info.DirFormToString(data);
	  ret = putdata(parent, data);
	  if (ret != yfs_client::OK) {
		  RestoreInum(ret_ino, true);
		  return ret;
	  }
	  data.clear();
	  ret = putdata(ret_ino, data);
	  if (ret != yfs_client::OK){
		  RestoreInum(ret_ino, true);
		  return ret;
	  }
	  yfs_client::fileinfo info;
	  ret = getfile(ret_ino, info);
	  if (ret != yfs_client::OK){
	  	  RestoreInum(ret_ino, true);
	  	  return ret;
	  }
	  return yfs_client::OK;
}
yfs_client::status yfs_client::readdir(DirForm& dirinfo, uint64_t inum){
	;
	std::string data;
	YfsLock yl(lc, inum);
	yfs_client::status ret = getdata(inum, data);
	if (ret != yfs_client::OK){
		return ret;
	}
	  dirinfo.StringToDirForm(data);
	  return yfs_client::OK;
}
yfs_client::status yfs_client::lookup(uint64_t parent, const char *name, bool& found, uint64_t& imun){
	  DirForm dirinfo;
	  std::string data;
	  std::string str_name = name;
	  str_name.resize(MAX_ITEM_NAME_LEN);
	  printf("input name=%s\n", str_name.c_str());
	  std::map<std::string, uint64_t>::iterator mit;
	  YfsLock yl(lc, parent);
	  if(getdata(parent, data) != yfs_client::OK){
		  return yfs_client::NOENT;
	  }

	  dirinfo.StringToDirForm(data);
	  printf("fuseserver_lookup dirinfo.dir_block_ size is %lu\n", dirinfo.dir_block_.size());
	  mit = dirinfo.dir_block_.begin();
	  /*
	  for(;mit != dirinfo.dir_block_.end(); mit++) {
		  printf("name:%s,in:%llu\n",mit->first.c_str(), mit->second);

	  }
	  */
	  if (dirinfo.dir_block_.count(str_name) > 0){
		  imun = dirinfo.dir_block_[str_name];
		  found = true;
	  }
	  return yfs_client::OK;
}
yfs_client::status yfs_client::setattr(uint64_t ino, struct stat *attr){
	  std::string data;
	  YfsLock yl(lc, ino);
	  yfs_client::status ret = getdata(ino, data);
	  if (ret != yfs_client::OK) {
		  return ret;
	  }
	  data.resize(attr->st_size);
	  ret = putdata(ino, data);
	  if (ret != yfs_client::OK) {
		  return ret;
	  }
	  return ret;
}

yfs_client::status yfs_client::mkdir(uint64_t parent, const char *name, uint64_t& ret_ino) {
	  yfs_client::status ret;
	  std::string data;
	  std::string str_name = name;
	  str_name.resize(MAX_ITEM_NAME_LEN);
	  printf("parent id: %lu\n", parent);
	  printf("file name is %s\n", str_name.c_str());
	  YfsLock yl(lc, parent);
	  ret = getdata(parent, data);
	  if (ret != yfs_client::OK){
		  return yfs_client::NOENT;
	  }
	  DirForm dir_info;
	  dir_info.StringToDirForm(data);
	  if (dir_info.dir_block_.count(str_name) > 0) {
		  return yfs_client::EXIST;
	  }
	  ret_ino = GetAvailInum(false);
	  printf("fuseserver_createhelper ino:%lu\n", ret_ino);
	  if (ret_ino == 0) {
		  return yfs_client::NOENT;
	  }
	  dir_info.dir_block_[str_name] = ret_ino;
	  dir_info.DirFormToString(data);
	  ret = putdata(parent, data);
	  if (ret != yfs_client::OK) {
		  RestoreInum(ret_ino, false);
		  return ret;
	  }
	  data.clear();
	  ret = putdata(ret_ino, data);
	  if (ret != yfs_client::OK){
		  RestoreInum(ret_ino, false);
		  return ret;
	  }
	  yfs_client::fileinfo info;
	  ret = getfile(ret_ino, info);
	  if (ret != yfs_client::OK){
		  RestoreInum(ret_ino, false);
		  return ret;
	  }
	  return yfs_client::OK;
}

yfs_client::status yfs_client::unlink(uint64_t parent, const char *name){
	  yfs_client::status ret;
	  std::string data;
	  std::string str_name = name;
	  str_name.resize(MAX_ITEM_NAME_LEN);
	  YfsLock yl(lc, parent);
	  ret = getdata(parent, data);
	  if (ret != yfs_client::OK){
		  return yfs_client::NOENT;
	  }
	  DirForm dir_info;
	  dir_info.StringToDirForm(data);
	  std::map<std::string, uint64_t>::iterator del_mit = dir_info.dir_block_.find(str_name);
	  //printf("unlink str-name:%s\n, parent is %llu\n", str_name.c_str(), parent);
	  if (del_mit == dir_info.dir_block_.end()) {
		  return yfs_client::NOENT;
	  }
	  uint64_t ino = del_mit->second;
	  //printf("yfs_client::unlink ino:%ullX\n", ino);
	  if (!isfile(ino)) {
		  return yfs_client::NOENT;
	  }
	  /*
	  int ret_restore = RestoreInum(true, ino);

	  if (ret_restore != 0) {
		  return yfs_client::NOENT;
	  }
	  */
	  dir_info.dir_block_.erase(del_mit);
	  dir_info.DirFormToString(data);
	  ret = putdata(parent, data);
	  if (ret != yfs_client::OK) {
		  return ret;
	  }
	  YfsLock yl2(lc, ino);
	  if (ec->remove(ino) != extent_protocol::OK) {
		return yfs_client::NOENT;
	  }

	return yfs_client::OK;
}


void DirForm::StringToDirForm(const std::string& data){
	const char* start = data.data();
	std::size_t pos = 0;
	dir_item_num_ = *(int *) (start + pos);
	printf("dir_item_num_ = %d\n", dir_item_num_);
	pos += sizeof(int);
	for(int i = 0; i < dir_item_num_ && pos < data.size(); i++) {
		std::string item_name;
		item_name.assign((char *) (start + pos), MAX_ITEM_NAME_LEN);
		pos += MAX_ITEM_NAME_LEN;
		uint64_t item_num = *(uint64_t *)(start + pos);
		pos += sizeof(uint64_t);
		dir_block_.insert(std::make_pair(item_name, item_num));
	}
}

void DirForm::DirFormToString(std::string& data){
	data.clear();
	dir_item_num_ = dir_block_.size();
	char a32[4];
	memcpy(a32, &dir_item_num_, 4);
	data.append(a32, sizeof(dir_item_num_));
	typeof(dir_block_.begin()) mit = dir_block_.begin();
	for(;mit != dir_block_.end(); mit++){
		std::string tmp_str = mit->first;
		tmp_str.resize(MAX_ITEM_NAME_LEN);
		data.append(tmp_str.c_str(), MAX_ITEM_NAME_LEN);
		char a64[8];
		memcpy(a64, &(mit->second), 8);
		data.append(a64, sizeof(mit->second));
	}
}

YfsLock::YfsLock(lock_client* clc, uint64_t inum)
	:lc(clc), ino(inum)
{
	lc->acquire(ino);
}
YfsLock::~YfsLock(){
	lc->release(ino);
}
