/**
 * @author Andreas Hager, andreas.hager@aisec.fraunhofer.de
*/

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#include "sanitizer_common/sanitizer_internal_defs.h"

#define SYSTEM_CALL_COUNT 330

extern "C" {
    SANITIZER_INTERFACE_ATTRIBUTE
    extern int containsReturnValue(int callID);

    SANITIZER_INTERFACE_ATTRIBUTE
    extern void addReturnValuei64(int callID, long returnValue);
    SANITIZER_INTERFACE_ATTRIBUTE
    extern long getReturnValuei64(int callID);

    SANITIZER_INTERFACE_ATTRIBUTE
    extern void addReturnValuei32(int callID, int returnValue);
    SANITIZER_INTERFACE_ATTRIBUTE
    extern int getReturnValuei32(int callID);
} // extern "C"
