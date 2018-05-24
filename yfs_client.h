#ifndef yfs_client_h
#define yfs_client_h

#include <string>
//#include "yfs_protocol.h"
#include "extent_client.h"
#include <vector>


#include "lock_protocol.h"
#include "lock_client.h"

#define MAX_ITEM_NAME_LEN (1024)
enum FileType{
	Tdir = 0,
	Tfile = 1
};

class YfsLock{
	lock_client* lc;
	uint64_t ino;
public:
	YfsLock(lock_client* lc, uint64_t inum);
	~YfsLock();
};

struct DirForm{
	std::map<std::string, uint64_t> dir_block_;
	int dir_item_num_;
	void StringToDirForm(const std::string&);
	void DirFormToString(std::string&);
};


class yfs_client {
  extent_client *ec;
  lock_client *lc;
  class lock_release_user *lu;
  pthread_mutex_t mutex_inode_map_;
 public:

  typedef unsigned long long inum;
  enum xxstatus { OK, RPCERR, NOENT, IOERR, EXIST };
  typedef int status;
  std::string lock_dst;
  struct fileinfo {
    unsigned long long size;
    unsigned long atime;
    unsigned long mtime;
    unsigned long ctime;
  };
  struct dirinfo {
    unsigned long atime;
    unsigned long mtime;
    unsigned long ctime;
  };
  struct dirent {
    std::string name;
    yfs_client::inum inum;
  };

  uint64_t FindIdelInodeNum(const uint64_t inode_bit_map_[]);
  int SetInodeBitMap(uint64_t inum, uint64_t inode_bit_map_[]);
  int UnSetInodeBitMap(uint64_t inum, uint64_t inode_bit_map_[]);
  uint64_t GetAvailInum(bool);
  uint64_t GetAvailInumNotUse(bool);
  int RestoreInum(bool, uint64_t);
  int RestoreInumNotUse(bool, uint64_t);
 private:
  static std::string filename(inum);
  static inum n2i(std::string);
 public:

  yfs_client(std::string, std::string);
  ~yfs_client();
  bool isfile(inum);
  bool isdir(inum);

  int getfile(inum, fileinfo &);
  int getdir(inum, dirinfo &);
  int getdata(inum, std::string &);
  int putdata(inum, std::string &);
  yfs_client::status create(uint64_t parent, const char *name, uint64_t& ret_ino);
  yfs_client::status readdir(DirForm& dirinfo, uint64_t inum);
  yfs_client::status lookup(uint64_t parent, const char *name, bool& found, uint64_t& imun);
  yfs_client::status setattr(uint64_t ino, struct stat *attr);
  yfs_client::status mkdir(uint64_t parent, const char *name, uint64_t& inum);
  yfs_client::status unlink(uint64_t parent, const char *name);
  yfs_client::status read(uint64_t ino, size_t size, off_t off, std::string &buf);
  yfs_client::status write(uint64_t ino, size_t size, off_t off, const char* buf);
};

#endif 
