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
typedef struct _fileStruct fileStruct;

int createOFT(void);
struct semaphore *OFTMutex;
int openFile (userptr_t, int, mode_t, int32_t*);
int closeFile (int32_t fd);

#endif /* _FILE_H_ */
