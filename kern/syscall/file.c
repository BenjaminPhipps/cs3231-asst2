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

#define maxOFTSize 128
/*
 * Add your file-related functions here ...
 put all OFT controlling shit in here
 
 */

fileStruct * oft;
//void createOFT();

void createOFT() {
    kprintf("intializing OFT here\n");
    oft = (fileStruct *) kmalloc(sizeof(fileStruct)* maxOFTSize);
    
    for (int i =0; i < maxOFTSize; i++){
        oft[i].permissions = -1;
        oft[i].freeFlag = 0;
        oft[i].refcount = 0;
        oft[i].vnodePtr = NULL;
    }
    kprintf("intialized OFT\n");
}
