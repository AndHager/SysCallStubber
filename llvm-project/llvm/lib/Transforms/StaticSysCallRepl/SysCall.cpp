#include "SysCall.h"

SysCall::SysCall(
    const int pSysCallID,
    const string pReturnType,
    const long pReturnValue
) : sysCallID(pSysCallID)
, returnType(pReturnType)
, returnValue(pReturnValue) {}

bool SysCall::operator==(const SysCall& that) const {
    return sysCallID == that.sysCallID
        && returnType == that.returnType;
}

ostream& operator<<(ostream& os, const SysCall& sc)
{
    os << "{ \"SysCallID\": \"";
    os << sc.sysCallID;
    os << "\", \"ReturnType\": \"";
    os << sc.returnType;
    os << "\", \"ReturnValue\": \"";
    os << sc.returnValue;
    os << "\" }\n";
    return os;
}