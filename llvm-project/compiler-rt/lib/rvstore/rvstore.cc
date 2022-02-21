/**
 * @author Andreas Hager, andreas.hager@aisec.fraunhofer.de
*/

#include "rvstore/rvstore.h"

typedef struct {
    int callSize = SYSTEM_CALL_COUNT + 1;
    long returnValues[SYSTEM_CALL_COUNT] = {LONG_MIN};
} Store;
Store store = {};

extern int containsReturnValue(int callID) {
    return store.callSize > callID 
            && callID >= 0 
            && store.returnValues[callID] != LONG_MIN;
}

// ----------------
// ---- INT 64 ----
// ----------------
extern void addReturnValuei64(int callID, long returnValues) {
    if (store.callSize > callID && callID >= 0) { 
        // Set return value
        store.returnValues[callID] = returnValues;
    }
}
extern long getReturnValuei64(int callID) {
    if (containsReturnValue(callID)) {
        // Get return value
        return store.returnValues[callID];
    }
    return 0;
}

// ----------------
// ---- INT 32 ----
// ----------------
extern void addReturnValuei32(int callID, int returnValues) {
    addReturnValuei64(callID, (long)returnValues);
}
extern int getReturnValuei32(int callID) {
    return (int)getReturnValuei64(callID);
}
