/*
 * Declarations for file handle and file table management.
 */

#ifndef _FILE_H_
#define _FILE_H_

/*
 * Contains some file-related maximum length constants
 */
#include <limits.h>
#include <vnode.h>

/*
 * Put your function declarations and data types here ...
 */
typedef struct _fileStruct {
    int permissions;
    int freeFlag;
    int refcount;
    struct vnode *vnodePtr;
} fileStruct;

void createOFT(void);
#endif /* _FILE_H_ */
