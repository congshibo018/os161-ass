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

extern struct OFtable_node *OFtable[OPEN_MAX];
extern struct spinlock *OFtable_lock;
int open(char *filename,int flags, mode_t mode){
  /* *vn = current opening file*/
  struct file *file;
  struct vnode *vn;
  int i,j; int errno;
  
  file = kmalloc(sizeof(struct file));
  KASSERT(file != NULL);
  if(file == NULL){
    errno = ENOMEM;
    return -errno;
  }

  int error_code = vfs_open(filename, flags, mode, &vn);
  if(error_code){
    errno = error_code;
    return -errno;
  }
  file->f_vnode = vn;
  file->f_lock = lock_create("file lock");
  for (i = 3; i < __OPEN_MAX; i++){
    if(curproc->FDtable[i] == NULL){
      break;/*i is the index in FDtable*/
    }
  }
  if(i > __OPEN_MAX - 1){
    errno = EMFILE;
    return -errno;
  }
  spinlock_acquire(OFtable_lock);
  int max_j =__OPEN_MAX;
  for (j = 0; j < max_j; j++){
    if(&OFtable[j] == NULL||OFtable[j]->ref_count == 0){
      break;/*j is the index in OFtable*/
    }
  }
  if(j > max_j - 1){
    errno = ENFILE;
    spinlock_release(OFtable_lock);
    return -errno;
  }
  OFtable[j]->file_node = file;
  OFtable[j]->offset = 0;
  OFtable[j]->ref_count = 1;
  OFtable[j]->mode = flags & O_ACCMODE;

  curproc->FDtable[i] = OFtable[j];
  spinlock_release(OFtable_lock);
  return i;
}

int close(int fd){
  int errno;
  if(fd<3|| fd>=OPEN_MAX|| curproc->FDtable[fd] == NULL){
    errno = EBADF;
    return -errno;
  }
  spinlock_acquire(OFtable_lock);
  curproc->FDtable[fd]->ref_count -= 1;
  spinlock_release(OFtable_lock);
  if(curproc->FDtable[fd]->ref_count == 0){
    vfs_close(curproc->FDtable[fd]->file_node->f_vnode);
    lock_destroy(curproc->FDtable[fd]->file_node->f_lock);
    kfree(curproc->FDtable[fd]->file_node);
    /*clean OFtable */
    curproc->FDtable[fd]->file_node = NULL;
    curproc->FDtable[fd]->offset = 0;
  }
  curproc->FDtable[fd] = NULL;
  return 0;
}

off_t lseek(int fd, off_t pos, int whence){
  struct stat fileStat;
  off_t new_pos;
  int errno;
  spinlock_acquire(OFtable_lock);
  new_pos = curproc->FDtable[fd]->offset;

  if(curproc->FDtable[fd] == NULL){
    errno = EBADF;
    spinlock_release(OFtable_lock);
    return -errno;
  }
  if(whence == SEEK_SET){
    new_pos = pos;
  }else if (whence == SEEK_CUR){
    new_pos += pos;
  }else if (whence == SEEK_END){
    VOP_STAT(curproc->FDtable[fd]->file_node->f_vnode,&fileStat);
    new_pos = fileStat.st_size + pos;
  }else{
    errno = EINVAL;
    spinlock_release(OFtable_lock);
    return -errno;
  }
  curproc->FDtable[fd]->offset = new_pos;
  spinlock_release(OFtable_lock);
  return new_pos;
}

int dup2(int oldfd, int newfd){
  int errno;
  /*leave fd0,1,2 alone*/
  if(oldfd < 0|| newfd < 3|| oldfd > OPEN_MAX - 1|| newfd > OPEN_MAX - 1){
    errno = EBADF;
    return -errno;
  }
  if(curproc->FDtable[oldfd] == NULL){
    errno = EBADF;
    return -errno;
  }
  if(curproc->FDtable[newfd] != NULL){
    errno = close(newfd);
    if(errno){
      return -errno;
    }
  }

  spinlock_acquire(OFtable_lock);
  curproc->FDtable[newfd] = curproc->FDtable[oldfd];
  curproc->FDtable[oldfd]->ref_count+=1;
  spinlock_release(OFtable_lock);

  return newfd;
}

ssize_t read(int fd, void *buf, size_t buflen){
    int errno;
    struct stat fileStat;
    if(fd < 3 || fd >=__OPEN_MAX || curproc ->FDtable[fd] == NULL){
        errno = EBADF;
        return -errno;
    } 
    spinlock_acquire(OFtable_lock);
    if(curproc->FDtable[fd]->mode == O_WRONLY|| curproc->FDtable[fd]->file_node == NULL){
        errno = EBADF;
	      spinlock_release(OFtable_lock);
        return -errno;
    }
    lock_acquire(curproc->FDtable[fd]->file_node->f_lock);
    spinlock_release(OFtable_lock);
    int err = VOP_STAT(curproc->FDtable[fd]->file_node->f_vnode,&fileStat);
    if(err){
      return -err;
    }
    if(curproc->FDtable[fd]->offset == fileStat.st_size){
      lock_release(curproc->FDtable[fd]->file_node->f_lock);
      return 0;
    }
  	struct iovec* iov;
  	struct uio* readUio;
    iov = (struct iovec*)kmalloc(sizeof(struct iovec));
    readUio = (struct uio*)kmalloc(sizeof(struct uio));
    
    uio_kinit(iov, readUio, buf, buflen, curproc->FDtable[fd]->offset, UIO_READ);

    //    readUio->uio_segflg = UIO_USERSPACE;

    err = VOP_READ(curproc->FDtable[fd]->file_node->f_vnode, readUio);
    lock_release(curproc->FDtable[fd]->file_node->f_lock);
    ssize_t size = readUio->uio_offset - curproc->FDtable[fd]->offset;
    if(err){
      errno = err;
      return -errno;
    }
    if(size < 0){
        errno = EIO;
        return -errno;
    }

    spinlock_acquire(OFtable_lock);
    curproc->FDtable[fd]->offset += size;
    spinlock_release(OFtable_lock);

    return size;
}



ssize_t write(int fd, void *buf, size_t nbytes){
    int errno;
    if(fd >=__OPEN_MAX || curproc ->FDtable[fd] == NULL){
        errno = EBADF;
        return -errno;
    }
    spinlock_acquire(OFtable_lock);
    if(fd > 2 && curproc->FDtable[fd]->mode == O_RDONLY){
      spinlock_release(OFtable_lock);
        errno = EBADF;
        return -errno;
    }
    lock_acquire(curproc->FDtable[fd]->file_node->f_lock);
    spinlock_release(OFtable_lock);
    struct iovec* iov;
    struct uio* writeUio;
    iov = (struct iovec*)kmalloc(sizeof(struct iovec));
    writeUio = (struct uio*)kmalloc(sizeof(struct uio));
    uio_kinit(iov, writeUio, buf, nbytes, curproc->FDtable[fd]->offset, UIO_WRITE);

    int err = VOP_WRITE(curproc->FDtable[fd]->file_node->f_vnode, writeUio);
    lock_release(curproc->FDtable[fd]->file_node->f_lock);
    ssize_t size = writeUio->uio_offset - curproc->FDtable[fd]->offset;
    if(err){
      errno = err;
      return -errno;
    }
    if(size < 0){
        errno = EIO;
        return -errno;
    }
    else{
      spinlock_acquire(OFtable_lock);
      curproc->FDtable[fd]->offset = writeUio->uio_offset;
      spinlock_release(OFtable_lock);
      return size;
    }
}

void OFtable_bootstrap(){
  int i,max_j;
  max_j = __OPEN_MAX;//OFtable use static size, can be changed if system require larger OFtable
  for(i=0;i<max_j;i++){
    OFtable[i] = (struct OFtable_node*)kmalloc(sizeof(struct OFtable_node));
  }
}

void OFtable_clean(){
  kfree(OFtable);
}

void OFtable_lock_bootstrap(){
  OFtable_lock = kmalloc(sizeof(struct spinlock));
  spinlock_init(OFtable_lock);
}
