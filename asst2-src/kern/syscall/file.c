#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <kern/limits.h>
#include <kern/stat.h>
#include <kern/seek.h>
#include <lib.h>
#include <uio.h>
#include <thread.h>
#include <proc.h>
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

extern struct OFtable_node *OFtable;
int open(char *filename,int flags, mode_t mode){
  /* *vn = current opening file*/
  struct file *file;
  struct vnode *vn;
  int i,j; int errno;
  
  file = kmalloc(sizeof(struct file));
  if(file == NULL){
    errno = ENOMEM;
    return -1;
  }

  int error_code = vfs_open(filename, flags, mode, &vn);
  if(error_code){
    errno = error_code;
    return -1;
  }
  file->f_vnode = vn;

  /*however where to define OFtable still not decided yet so we just implement FDtable first*/
  for (i = 3; i < __OPEN_MAX; i++){
    if(curproc->FDtable[i] == NULL){
      break;/*i is the index in FDtable*/
    }
  }
  if(i > __OPEN_MAX - 1){
    errno = EMFILE;
    return -1;
  }

  int max_j = 32;
  for (j = 0; j < max_j; j++){
    if(&OFtable[j] == NULL){
      break;/*j is the index in OFtable*/
    }
  }
  if(j > max_j - 1){
    errno = ENFILE;
    return errno;
  }
  OFtable[j].file_node = file;
  OFtable[j].offset = 0;
  OFtable[j].ref_count = 1;
  OFtable[j].mode = flags & O_ACCMODE;

  curproc->FDtable[i] = &OFtable[j];
  return 0;
}

int close(int fd){
  int errno;
  if(fd<3|| fd>=OPEN_MAX|| curproc->FDtable[fd] == NULL){
    errno = EBADF;
    return errno;
  }
  curproc->FDtable[fd]->ref_count -= 1;
  if(curproc->FDtable[fd]->ref_count == 0){
    vfs_close(curproc->FDtable[fd]->file_node->f_vnode);
    kfree(curproc->FDtable[fd]->file_node);
    /*clean OFtable */
    curproc->FDtable[fd]->file_node = NULL;
    curproc->FDtable[fd]->offset = 0;
  }
  curproc->FDtable[fd] = NULL;
  return 0;
}

off_t lseek(int fd, off_t pos, int whence){
  struct stat *fileStat = NULL;
  off_t new_pos;
  int errno;
  new_pos = curproc->FDtable[fd]->offset;

  if(curproc->FDtable[fd] == NULL){
    errno = EBADF;
    return errno;
  }
  if(whence == SEEK_SET){
    new_pos = pos;
  }else if (whence == SEEK_CUR){
    new_pos += pos;
  }else if (whence == SEEK_END){
    VOP_STAT(curproc->FDtable[fd]->file_node->f_vnode,fileStat);
    new_pos = fileStat->st_size + pos;
  }else{
    errno = EINVAL;
    return errno;
  }
  curproc->FDtable[fd]->offset = new_pos;
  return new_pos;
}

int dup2(int oldfd, int newfd){
  int errno;
  /*leave fd0,1,2 alone*/
  if(oldfd < 0|| newfd < 3|| oldfd > OPEN_MAX - 1|| newfd > OPEN_MAX - 1){
    errno = EBADF;
    return errno;
  }
  if(curproc->FDtable[oldfd] == NULL){
    errno = EBADF;
    return errno;
  }
  if(curproc->FDtable[newfd] != NULL){
    errno = close(newfd);
    if(errno){
      return errno;
    }
  }
  curproc->FDtable[newfd] = curproc->FDtable[oldfd];
  return newfd;
}

ssize_t read(int fd, void *buf, size_t buflen){
    int errno;
    if(fd < 3 || fd >=__OPEN_MAX || curproc ->FDtable[fd] == NULL){
        errno = EBADF;
        return -1;
    } 

    if(curproc->FDtable[fd]->mode == O_WRONLY){
        errno = EBADF;
        return -1;
    }

  	struct iovec* iov;
  	struct uio* readUio;
    iov = (struct iovec*)kmalloc(sizeof(struct iovec));
    readUio = (struct uio*)kmalloc(sizeof(struct uio));
    
    uio_kinit(iov, readUio, buf, buflen, curproc->FDtable[fd]->offset, UIO_READ);

    readUio->uio_segflg = UIO_USERSPACE;

    ssize_t size = VOP_READ(curproc->FDtable[fd]->file_node->f_vnode, readUio);

    if(size < 0){
        errno = EIO;
        return errno;
    }
    else{
        return size;        
    }
}



ssize_t write(int fd, void *buf, size_t nbytes){
    int errno;
    if(fd < 3 || fd >=__OPEN_MAX || curproc ->FDtable[fd] == NULL){
        errno = EBADF;
        return -1;
    } 

    if(curproc->FDtable[fd]->mode == O_RDONLY){
        errno = EBADF;
        return -1;
    }

    struct iovec* iov;
    struct uio* writeUio;
    iov = (struct iovec*)kmalloc(sizeof(struct iovec));
    writeUio = (struct uio*)kmalloc(sizeof(struct uio));

    uio_kinit(iov, writeUio, buf, nbytes, curproc->FDtable[fd]->offset, UIO_READ);

    writeUio->uio_segflg = UIO_USERSPACE;

    ssize_t size = VOP_WRITE(curproc->FDtable[fd]->file_node->f_vnode, writeUio);

    if(size < 0){
        errno = EIO;
        return errno;
    }
    else{
        return size;        
    }
}

void OFtable_bootstrap(){
  int max_j;
  max_j = 32;
  OFtable = (struct OFtable_node*)kmalloc(max_j*sizeof(struct OFtable_node));
  
}

void OFtable_clean(){
  kfree(OFtable);
}
