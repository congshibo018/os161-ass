#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <kern/limits.h>
#include <kern/stat.h>
#include <kern/seek.h>
#include <lib.h>
#include <uio.h>
#include <thread.h>
#include <current.h>
#include <synch.h>
#include <vfs.h>
#include <vnode.h>
#include <file.h>
#include <syscall.h>
#include <copyinout.h>

/*
 * Add your file-related functions here ...
 */
int open(const char *filename,int flags){
  /* *vn = current opening file*/
  struct file *file;
  struct vnode *vn;
  int i,j;
  file = kmalloc(sizeof(struct file));
  if(file == NULL){
    errno = ENOMEM;
    return -1;
  }
  int error_code = vfs_open(filename, flag, &vn);
  if(error_code){
    errno = error_code;
    return -1;
  }
  file->f_vnode = vn;

  /*however where to define OFtable still not decided yet so we just implement FDtable first*/
  for (i = 3; i <= __OPEN_MAX; i++){
    if(curthread->FDtable[i] == NULL){
      break;/*i is the index in FDtable*/
    }
  }
  if(i > __OPEN_MAX){
    errno = EMFILE;
    return -1;
  }

  int max_j = (__PID_MAX - __PID_MIN)*__OPEN_MAX;
  for (j = 0; j <= max_j; j++){
    if(OFtable[j] == NULL){
      break;/*j is the index in OFtable*/
    }
  }
  if(j > max_j){
    errno = ENFILE;
    return -1;
  }
  OFtable[j]->file_node = file;/*OFtable not defined yet */
  OFtable[j]->offset = 0;

  curthread->FDtable[i] = OFtable[j];
  return 0;
}

int close(int fd){
  int i;
  if(fd<3|| fd>=OPEN_MAX|| curthread->FDtable[fd] == NULL){
    errno = EBADF;
    return -1;
  }
  vfs_close(curthread->FDtable[fd]->file_node->f_vnode);
  curthread->FDtable[fd] = NULL;
  kfree(curthread->FDtable[fd]->file_node);
  /*clean OFtable */
  curthread->FDtable[fd]->file_node = NULL;
  curthread->FDtable[fd]->offset = NULL;

  /*clean FDtable */
  curthread->FDtable[fd] = NULL;
  return 0;
  /*operation on OFtable needed*/
}
