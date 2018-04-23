/*
 * Declarations for file handle and file table management.
 */

#ifndef _FILE_H_
#define _FILE_H_

/*
 * Contains some file-related maximum length constants
 */
#include <limits.h>

struct file {
  struct vnode *f_vnode;
};

struct OFtable_node{
  struct file *file_node;
  int offset;
};

struct OFtable_node *FDtable[__OPEN_MAX];
/*
 * Put your function declarations and data types here ...
 */

int open(const char *filename,int flag);
int close(int fd);
#endif /* _FILE_H_ */
