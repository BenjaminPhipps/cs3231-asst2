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
    unsigned int offset;
    struct vnode *vnodePtr;
};

/*
 * Add your file-related functions here ...
 put all OFT controlling shit in here

 */

fileStruct *oft;

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
    return 0;
}

int openFile (userptr_t filename, int flags, mode_t mode, int32_t* retval) {
        int error = 0;
        kprintf("filename is %s, flags are %d \n", (char *) filename, flags);
        if (flags == O_RDONLY) {
                kprintf("opening for read only\n");
        } else if (flags == O_WRONLY) {
                kprintf("opening for write only\n");
        } else if (flags == O_RDWR) {
                kprintf("opening for read or write\n");
        }

        struct vnode *_vnode;
        error = vfs_open ((char *) filename, flags, mode, &_vnode);
        if (error != 0) {
          // handle error
        }

        if (!VOP_ISSEEKABLE(_vnode)) {
          return EFAULT;
        }

        // TODO: obtain OFT mutex

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

        // TODO: release OFT mutex

        if (OFTIndex == -1) {
          return ENFILE;
        }

        error = proc_newFD(OFTIndex, retval);

        return error;
}

int closeFile (int32_t fd) {
  int error = 0;
  int i;

  error = proc_getOFTIndex(fd, &i);
  if (error != 0) {
    return error;
  }

  // TODO: obtain OFT mutex

  if (oft[i].freeFlag != 1) {
    panic("OFT is inconsistent with process fd table\n");
  }

  error = proc_removeFD(fd);
  if (error != 0) {
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

  // TODO: release OFT mutex;

  return error;
}
