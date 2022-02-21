#include "llvm/IR/InstrTypes.h"
#include <list>
#include <iostream>

using namespace llvm;
using namespace std;

class SysCall {
    public:
        SysCall(
            const int pSysCallID,
            const string pReturnType,
            const long pReturnValue
        );

        bool operator==(const SysCall &that) const;

        friend ostream& operator<<(ostream& os, const SysCall& sc);

        const int sysCallID;
        const string returnType;
        const long returnValue;      
};

namespace std {
  template <> struct hash<SysCall>
  {
    size_t operator()(const SysCall& x) const
    {
      return x.sysCallID
        + 151 * hash<string>()(x.returnType);
    }
  };
}