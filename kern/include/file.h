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
int writeToFile (int32_t fd, const void *buf, size_t nbytes, int32_t *retval);
int readFromFile (int32_t fd, void *buf, size_t nbytes, int32_t *retval);
int seekFilePos (int32_t fd, off_t pos, int whence, off_t *retval);

#endif /* _FILE_H_ */
