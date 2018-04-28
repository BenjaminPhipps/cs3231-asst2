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
#include <proc.h>

#define OFT_MAX 128

struct _fileStruct {
    mode_t permissions;
    int freeFlag;
    int refcount;
    off_t offset;
    struct vnode *vnodePtr;
};

/*
 * Add your file-related functions here ...
 put all OFT controlling shit in here

 */

fileStruct *oft;
struct semaphore *OFTMutex;

int createOFT() {
    oft = (fileStruct *) kmalloc(sizeof(fileStruct)* OFT_MAX);
    if (oft == NULL) {
      return ENOMEM;
    }

    for (int i =0; i < OFT_MAX; i++) {
        oft[i].permissions = -1;
        oft[i].freeFlag = 0;
        oft[i].refcount = 0;
        oft[i].offset = 0;
        oft[i].vnodePtr = NULL;
    }
    oft[1].permissions = -1; //what do I do here?
    oft[1].freeFlag = 1;
    oft[1].refcount = 1;
    oft[1].offset = 0;
    oft[1].vnodePtr = NULL;

    char c1[] = "con:";
    char c2[] = "con:";


    vfs_open(c1,2,(mode_t) 0,&oft[1].vnodePtr); 
    vfs_open(c2,2,(mode_t) 0,&oft[2].vnodePtr); 
    return 0;
}

int openFile (userptr_t filename, int flags, mode_t mode, int32_t* retval) { //remoce mode_t as it is useless
        int error = 0;
        //kprintf("filename is %s, flags are %d \n", (char *) filename, flags);
/*
        if (flags == O_RDONLY) {                    // all these flag checks are wrong
                kprintf("opening for read only\n");
        } else if (flags == O_WRONLY) {
                kprintf("opening for write only\n");
        } else if (flags == O_RDWR) {
                kprintf("opening for read or write\n");
        } else {
                kprintf("flags undetermined\n");
        }
*/
        struct vnode *_vnode;
        error = vfs_open ((char *) filename, flags, mode, &_vnode);
        if (error != 0) {
                kprintf("can't open, vfs node error\n");    
                return error;
        }

        if (!VOP_ISSEEKABLE(_vnode)) {
                kprintf("can't open, not seekable\n");  
                return EFAULT;
        }

        // obtain OFT mutex
        P(OFTMutex);

        // place vnode in OFT
        int OFTIndex = -1;
        for (int i = 0; i < OFT_MAX; i++) {
          if (oft[i].freeFlag == 0) {
            oft[i].permissions = mode;
            oft[i].vnodePtr = _vnode;
            oft[i].refcount = 1;
            oft[i].freeFlag = 1;
            OFTIndex = i;
            break;
          }
        }

        // release OFT mutex
        V(OFTMutex);
        if (OFTIndex == -1) {
                kprintf("can't open, OFT index of open file is -1\n");          
                return ENFILE;
        }

        error = proc_newFD(OFTIndex, retval); //unused retval
        if (error == 0){
            kprintf("opened retval %d successfully\n", OFTIndex);    
        }
        return error;
}

int closeFile (int32_t fd) {
  int error = 0;
  int i;

  error = proc_getOFTIndex(fd, &i);
  if (error != 0) {
    return error;
  }

  // obtain OFT mutex
  P(OFTMutex);

  if (oft[i].freeFlag != 1) {
    panic("OFT is inconsistent with process fd table\n");
  }

  error = proc_removeFD(fd);
  if (error != 0) {
        kprintf("can't close, proc_removeFD(fd) error\n");  
        return error;
  }

  if (oft[i].refcount == 1) {
    // close file
    vfs_close (oft[i].vnodePtr);
    oft[i].freeFlag = 0;
    oft[i].offset = 0;
    oft[i].permissions = -1;
    oft[i].vnodePtr = NULL;
  }

  oft[i].refcount--;

  // release OFT mutex;
  V(OFTMutex);
  return error;
}

int writeToFile (int32_t fd, const void *buf, size_t nbytes, int32_t *retval) {
  int error = 0;

  // check if directory or symlink
  // kern/stat.h, kern/stattypes.h
  // use vop_stat()

  if (nbytes == 0){
    return error;
  }

  int i;
  error = proc_getOFTIndex (fd, &i);
  if (error != 0) {
        kprintf("can't write, proc_getOFTIndex (fd, &i) error\n");  
        return error;
  }

  //kprintf("oft index = %d, vnodePtr = %p\n", i, oft[i].vnodePtr);

  // check if opened with write permissions
/*
  void *kernbuf = kmalloc(nbytes);
  if (kernbuf == NULL) {
        kprintf("can't write, out of kern memory\n");  
    return ENOSPC;
  }
*/
    //kprintf("pre dec\n");

  char * kernbuf[nbytes];
  error = copyin(buf, kernbuf, nbytes);
  if (error != 0) {
        kprintf("can't write, copyin error, number: %d\n", error);
        kprintf("size is %d, buf is %p \n", nbytes, buf);    
    return error;
  }

  struct iovec iov;
  struct uio myuio;

  uio_kinit(&iov, &myuio, kernbuf, nbytes, oft[i].offset, UIO_WRITE);

  myuio.uio_segflg = UIO_SYSSPACE;

  error = VOP_WRITE (oft[i].vnodePtr, &myuio);
  if (error != 0) {
        kprintf("can't write, VOP_WRITE (oft[i].vnodePtr, &myuio) error\n");  
    return error;
  }

  *retval = (int32_t) (nbytes - (int) myuio.uio_resid);

  //kfree(kernbuf);

  return error;
}
