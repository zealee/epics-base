/* os/vxWorks/osdMutex.h */

/* Author:  Marty Kraimer Date:    25AUG99 */

/********************COPYRIGHT NOTIFICATION**********************************
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
****************************************************************************/

#include <vxWorks.h>
#include <semLib.h>

/* If the macro is replaced by inline it is necessary to say
   static __inline__
   but then a warning message appears everywhere osdMutex.h is included
*/

#define epicsMutexUnlock(ID) semGive((SEM_ID)(ID))

#define epicsMutexLock(ID) \
(semTake((SEM_ID)(ID),WAIT_FOREVER)==OK ? epicsMutexLockOK : epicsMutexLockError)
