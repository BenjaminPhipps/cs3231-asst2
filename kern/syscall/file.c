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
#include <kern/stattypes.h>

#define OFT_MAX 128

struct _fileStruct {
    int permissions;
    int freeFlag;
    int refcount;
    off_t offset;
    struct vnode *vnodePtr;
};

fileStruct *oft;
struct semaphore *OFTMutex;

int isDirectoryOrSymlink (struct vnode *_vnode);

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
        // kprintf("Open flags are %d\n", flags);

        struct vnode *_vnode;
        error = vfs_open ((char *) filename, flags, mode, &_vnode);
        if (error != 0) {
                kprintf("ERROR: can't open, virtual node error\n");
                return error;
        }

        if (!VOP_ISSEEKABLE(_vnode)) {
                kprintf("ERROR: can't open, not openable\n");
                return EFAULT;
        }

        // obtain OFT mutex
        P(OFTMutex);

        // place vnode in OFT
        int OFTIndex = -1;
        for (int i = 0; i < OFT_MAX; i++) {
          if (oft[i].freeFlag == 0) {
            oft[i].permissions = flags;
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
                kprintf("ERROR: can't open, OFT index of open file is -1\n");
                return ENFILE;
        }

        error = proc_newFD(OFTIndex, retval);
        if (error == 0){
            kprintf("opened file successfully: OFT index = %d, fd = %d\n", OFTIndex, *retval);
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
          kprintf("ERROR: can't close, proc_removeFD(fd) error\n");
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


    if (nbytes == 0){
          return error;
    }

    int i;
    error = proc_getOFTIndex (fd, &i);
    if (error != 0) {
          kprintf("ERROR: can't write, proc_getOFTIndex (fd, &i) error\n");
          return error;
    }

    //kprintf("oft index = %d, vnodePtr = %p\n", i, oft[i].vnodePtr);

    // check if opened with write permissions
    if ((oft[i].permissions & O_WRONLY) == 0 && (oft[i].permissions & O_RDWR) == 0) { // not sure if correct
          kprintf("ERROR: Can't write, file is not opened with write permissions\n");
          return EBADF;
    }

    // check if directory or symbolic link
    if (isDirectoryOrSymlink(oft[i].vnodePtr)) {
      kprintf("ERROR: Can't write, file is incompatible to writing\n");
      return EBADF;
    }

    char *kernbuf = (char *) kmalloc(nbytes);
    if (kernbuf == NULL) {
          kprintf("ERROR: can't write, out of kern memory\n");
          return ENOSPC;
    }

    // char * kernbuf[nbytes];
    error = copyin(buf, kernbuf, nbytes);
    if (error != 0) {
          kprintf("ERROR: can't write, copyin error, number: %d\n", error);
          kprintf("size is %d, buf is %p \n", nbytes, buf);
          return error;
    }

    struct iovec iov;
    struct uio myuio;

    uio_kinit(&iov, &myuio, kernbuf, nbytes, oft[i].offset, UIO_WRITE);

    myuio.uio_segflg = UIO_SYSSPACE;

    error = VOP_WRITE (oft[i].vnodePtr, &myuio);
    if (error != 0) {
          kprintf("ERROR: can't write, VOP_WRITE error\n");
          return error;
    }

    *retval = (int32_t) (nbytes - (int) myuio.uio_resid);

    kfree(kernbuf);

    return error;
}

int readFromFile (int32_t fd, void *buf, size_t nbytes, int32_t *retval){
    int error = 0;
    //kprintf("fd = %d, bufPTR = %p, nbytes: %d\n", fd, buf, nbytes);

    if (nbytes == 0){
          return error;
    }

    int i;
    error = proc_getOFTIndex (fd, &i);
    if (error != 0) {
          kprintf("ERROR: can't read, proc_getOFTIndex error\n");
          return error;
    }

    /*
    // check if opened with read permissions
    if ((oft[i].permissions & O_RDONLY) == 0 && (oft[i].permissions & O_RDWR) == 0) { // not sure if correct
          kprintf("ERROR: Can't read, file is not opened with read permissions\n");
          kprintf("Permissions = %d\n", oft[i].permissions);
          return EBADF;
    }
    */

    // check if directory or symbolic link
    if (isDirectoryOrSymlink(oft[i].vnodePtr)) {
      kprintf("ERROR: Can't write, file is incompatible to writing\n");
      return EBADF;
    }

    struct iovec iov;
    struct uio myuio;

    uio_kinit(&iov, &myuio, buf, nbytes, oft[i].offset, UIO_READ);

    myuio.uio_segflg = UIO_SYSSPACE;
    error = VOP_READ(oft[i].vnodePtr, &myuio);
    if (error != 0) {
          kprintf("ERROR: can't read, VOP_READ error\n");
          return error;
    }

    *retval = (int32_t) (nbytes - (int) myuio.uio_resid);
    return error;
}

// check if vnode refers to a directory or symlink
int isDirectoryOrSymlink (struct vnode *_vnode) {
   struct stat fileInfo;
   if (VOP_STAT(_vnode, &fileInfo) != 0) {
        return 1;
   }
   if ((fileInfo.st_mode & _S_IFMT) == _S_IFDIR || (fileInfo.st_mode & _S_IFMT) == _S_IFLNK) {
        return 1;
   }
   return 0;
}
