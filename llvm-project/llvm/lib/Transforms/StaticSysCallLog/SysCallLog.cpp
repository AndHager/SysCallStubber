/**
 * @author Andreas Hager, andreas.hager@aisec.fraunhofer.de
*/

#include <fcntl.h>
#include <sstream>
#include <iostream>
#include <fstream>
#include <vector>
#include "llvm/IR/Function.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/PassRegistry.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

using namespace llvm;
using namespace std;

namespace {
  struct SysCallLog : public ModulePass {
    public:
      static char ID; // Pass identification, replacement for typeid
      SysCallLog() : ModulePass(ID) {}
      

      bool runOnModule(Module &M) override {
        list<CallLocation*> calls = {};

        for (Function &F : M) {
          for (BasicBlock &B : F) {
            for (Instruction &I : B) {
              CallInst *CI = dyn_cast<CallInst>(&I);
              if (CI != nullptr && isSystemCall(CI)) {
                calls.push_back(new CallLocation{ &M, &F, &B, CI });
              }
            }
          }
        }
        if (calls.size() > 0) {
          Components *C = init(&M);

          for(CallLocation *CL : calls) {
            modifyCall(CL, C);
          }
        }
        return true;
      }

    private:
      struct CallLocation {
        Module *M;
        Function *F;
        BasicBlock *B; 
        CallInst *CI;
      };
      struct Components {
        /* OPEN: */
        Constant *fileName;
        ConstantInt *oFlag;
        const FunctionCallee openCallee;

        /* WRITE: */
        Constant *emptyInitStr11;
        Constant *emptyInitStr22;
        Constant *formatStrInt;
        Constant *formatStrLong;
        const FunctionCallee writeCallee;

        /* CLOSE: */
        Constant *cCloseBrackStr;
        ConstantInt *cCloseBrackStrCount;
        const FunctionCallee closeCallee;
      };
      vector<string> sysCalls = {
        /*Process identification*/
        "getuid", "getgid", "getsid",
        "setuid", "setgid", "setsid",
        "getpgid", "getpid", "getppid", "getpgrp",
        "setpgid", "setreuid", "setresuid", "setregid", "setresgid", 
        "getresuid", "getresgid",
        "setgroups", "getgroups", "setfsuid", "setfsgid", "gettid",
        /*Inode attributes*/
        "lremovexattr", "fremovexattr", "lsetxattr", "fsetxattr",
        /*File ownership and mode*/
        "fchmod", "chmod", "chmodat", "fchmodat", "fchown", "chown", "chownat", "fchownat",
        /*Cache synchronization*/
        "sync", "syncfs",
        /*File synchronization*/
        "fsync", "fdatasync",
        /*Swapping mechanism*/
        "swapon", "swapoff",
        /*Sleep operation*/
        "nanosleep", "sleep",
        /*Scheduler operations*/
        "sched_getparam", "sched_setparam",
        "sched_getscheduler", "sched_setscheduler",
        "sched_get_priority_min", "sched_get_priority_max",
        "sched_getaffinity", "sched_setaffinity",
        "sched_rr_get_interval",
        /*Timer operation*/
        "timer_gettime", "timer_settime", "timer_getoverrun",
        "timer_create", "timer_delete", "gettimeofday", "gettimeofday",
        /*Clock operations*/
        "clock_nanosleep", "clock_gettime", "clock_settime",
        "clock_getres", "clock_adjtime",
        /*Epoll operations*/
        "epoll_create", "epoll_create1", "epoll_ctl", 
        "epoll_wait",
        /*Network operations*/
        "socket", "getsockopt", "setsockopt", "getsockname",
        "shutdown", "socketpair", 
        "listen", "bind", "getpeername", "connect", 
        "accept", "accept4", 
        // "open", "close",
        /*Sending/Receiving operations*/
        "sendto", "sendmsg", "sendmmsg", "recvfrom", "recvmsg", "recvmmsg",
        // "read", "write",
        /*Selected file operations*/
        "flock", "ftruncate", "truncate",
        /*etc*/
        "rand", "gethostname", "sethostname", "getrlimit", "setrlimit", "prlimit", "uname"
      };

      int getSystemCallID(CallInst *CI) {
        Function *calledFunction = CI->getCalledFunction();
        if (calledFunction != nullptr) {
          StringRef name = calledFunction->getName();
          string functionName = name.str();

          for (int index = 0; index < (int)sysCalls.size(); index++) {
            if (sysCalls[index] == functionName) {
              return index;
            }
          }
        }
        return -1;
      }

      bool isSystemCall(CallInst *CI) {
        return getSystemCallID(CI) > 0;
      }

      void modifyCall(CallLocation *CL, Components *CP) {
        Function *cf = CL->CI->getCalledFunction();
        if (cf == nullptr) return;

        string type_str;
        llvm::raw_string_ostream rso(type_str);
        cf->getReturnType()->print(rso);
        string cfRetrunTypeString = rso.str();
        string file = CL->M->getName().str();
        const int sysCallID = getSystemCallID(CL->CI);
        
        if (cfRetrunTypeString == "void") {
          CL->CI->eraseFromParent();
          errs() << "\n__INFO__: Erased call: \n";
          errs() << "\t{ \"File\": \"" << file;
          errs() << "\", \"Function\": \"" << CL->F->getName().str();
          errs() << ", \"CalledFunc\": \"" << cf->getName().str();
          errs() << ", \"SysCallID\": \"" << sysCallID;
          errs() << "\", \"ReturnType\": \"" << cfRetrunTypeString;
          errs() << "\" }\n";
          return;
        }

        const bool isI32 = cfRetrunTypeString == "i32";
        if (!isI32 && cfRetrunTypeString != "i64") {
          errs() << "\n__ERROR__: Abort call modification for: \n";
          errs() << "\t{ \"File\": \"" << file;
          errs() << "\", \"Function\": \"" << CL->F->getName().str();
          errs() << ", \"CalledFunc\": \"" << cf->getName().str();
          errs() << ", \"SysCallID\": \"" << sysCallID;
          errs() << "\", \"ReturnType\": \"" << cfRetrunTypeString;
          errs() << "\" }\n";
          return;
        }

        string base_filename = file.substr(file.find_last_of("/\\") + 1);
        size_t const dotIndex(base_filename.find_last_of('.'));
        string fn = base_filename.substr(0, dotIndex);
        string filename("/info/static_log/stubbed/" + fn + "_stubbed_calls.json");
        fstream file_out;

        file_out.open(filename, ios_base::app);
        if (!file_out.is_open()) {
            errs() << "\n__ERROR__: failed to open: " << filename << '\n';
        } else {
            file_out << "{ \"File\": \"" << file;
            file_out << "\", \"Function\": \"" << CL->F->getName().str();
            file_out << "\", \"CallID\": " << getSystemCallID(CL->CI);
            file_out << "\", \"CalledFunc\": \"" << cf->getName().str();
            file_out << "\", \"SysCallID\": \"" << sysCallID;
            file_out << "\", \"ReturnType\": \"" << cfRetrunTypeString;
            file_out << "\" }\n";
        }

        file_out.close();
        
        IRBuilder<> *builder = new IRBuilder<>(CL->CI);
        Instruction *InsertBefore = CL->CI->getNextNode();
        builder->SetInsertPoint(InsertBefore);
        logToJSON(builder, CL, CP);
      }

      Components *init(Module *M) {
        Function *f = &*M->begin();
        BasicBlock *b = &*f->begin();
        IRBuilder<> *startBuilder = new IRBuilder<>(&*b->begin());

        // -----------------------------------------------
        // ---- Open Preparation--------------------------
        // -----------------------------------------------
        Constant *fileName = startBuilder->CreateGlobalStringPtr("/info/ReturnValues.json", ".str");
        ConstantInt *oFlag = startBuilder->getInt32(O_WRONLY | O_CREAT | O_APPEND | O_SYNC);
        const FunctionCallee openCallee = M->getOrInsertFunction("open", 
          FunctionType::getInt32Ty(M->getContext()),
          Type::getInt8PtrTy(M->getContext()), 
          Type::getInt32Ty(M->getContext())
        );

        // ------------------------------------------------
        // ---- Write Preparation--------------------------
        // ------------------------------------------------
        ostringstream outStringStreamInit;
        for (int i = 0; i < 10; i++) outStringStreamInit << "-";
        Constant *emptyInitStr11 = startBuilder->CreateGlobalStringPtr(outStringStreamInit.str(), ".str");
        for (int i = 0; i < 10; i++) outStringStreamInit << "-";
        Constant *emptyInitStr22 = startBuilder->CreateGlobalStringPtr(outStringStreamInit.str(), ".str");

        Constant *formatStrInt = startBuilder->CreateGlobalStringPtr("%d", ".str");
        Constant *formatStrLong = startBuilder->CreateGlobalStringPtr("%ld", ".str");

        const FunctionCallee writeCallee = M->getOrInsertFunction("write",
          FunctionType::getInt64Ty(M->getContext()),
          Type::getInt32Ty(M->getContext()),
          Type::getInt8PtrTy(M->getContext()),
          Type::getInt64Ty(M->getContext())
        );

        // ------------------------------------------------
        // ---- Close Preparation--------------------------
        // ------------------------------------------------
        const string closeBrackStr = "\" }\n";
        Constant *cCloseBrackStr = startBuilder->CreateGlobalStringPtr(closeBrackStr, ".str");
        ConstantInt *cCloseBrackStrCount = startBuilder->getInt64(closeBrackStr.size());
        const FunctionCallee closeCallee = M->getOrInsertFunction("close", 
          FunctionType::getInt32Ty(M->getContext()), 
          Type::getInt32Ty(M->getContext())
        );

        return new Components {
          fileName,
          oFlag,
          openCallee,

          emptyInitStr11, 
          emptyInitStr22,
          formatStrInt,
          formatStrLong,
          writeCallee,

          cCloseBrackStr,
          cCloseBrackStrCount,
          closeCallee
        };
      }

      void logToJSON(IRBuilder<> *builder, CallLocation *CL, Components *CP) {
        Function *cf = CL->CI->getCalledFunction();
        if (cf == nullptr) return;

        const int sysCallID = getSystemCallID(CL->CI);

        // -----------------------------------------------------------
        // ---- Append Open Call -------------------------------------
        // -----------------------------------------------------------
        // TODO: ADD return value check
        Value *fileDescriptor = builder->CreateCall(
          CP->openCallee, 
          ArrayRef<Value *>{CP->fileName, CP->oFlag}, 
          "fileDescriptor"
        );

        // ------------------------------------------------------------
        // ---- Append Write Call -------------------------------------
        // ------------------------------------------------------------
        string file = CL->M->getName().str();
        string type_str;
        llvm::raw_string_ostream rso(type_str);
        cf->getReturnType()->print(rso);
        string cfRetrunTypeString = rso.str();
        ostringstream outStringStream;
        outStringStream << "{ \"File\": \"" << file;
        outStringStream << "\", \"Function\": \"" << CL->F->getName().str();
        outStringStream << "\", \"CalledFunc\": \"" << cf->getName().str();
        outStringStream << "\", \"SysCallID\": \"" << sysCallID;
        outStringStream << "\", \"ReturnType\": \"" << cfRetrunTypeString;
        outStringStream << "\", \"ReturnValue\": \"";
        string str = outStringStream.str();
        Constant *cstr = builder->CreateGlobalStringPtr(str, ".str");
        ConstantInt *strCount = builder->getInt64(str.size());
        builder->CreateCall(CP->writeCallee, ArrayRef<Value *>{
          fileDescriptor, 
          cstr,
          strCount
          }, "writeLocRet");
        
        int arraySize = -1;
        string format = "";
        Constant *emptyInitStr;
        Constant *formatStr;
        if (rso.str() == "i32") {
          arraySize = 11;
          formatStr = CP->formatStrInt;
          emptyInitStr = CP->emptyInitStr11;
        } else {
          arraySize = 22;
          formatStr = CP->formatStrLong;
          emptyInitStr = CP->emptyInitStr22;
        }

        // Init output String
        Value *buffStr = builder->CreateAlloca(
          ArrayType::get(
            Type::getInt8Ty(CL->M->getContext()), 
            arraySize
          ), 
          nullptr,
          "bufferString"
        );

        auto buffStrElemPtr = GetElementPtrInst::Create(
          ArrayType::get(
            Type::getInt8Ty(CL->M->getContext()), 
            arraySize
          ), 
          buffStr, 
          ArrayRef< Value * >{builder->getInt32(0)}, 
          "buffStrPtr",
          &*builder->GetInsertPoint()
        );
        const FunctionCallee initEmptyStringCallee = CL->M->getOrInsertFunction("sprintf",
          FunctionType::getInt8PtrTy(CL->M->getContext()),
          buffStrElemPtr->getType(),
          FunctionType::getInt8PtrTy(CL->M->getContext()),
          Type::getInt64Ty(CL->M->getContext())
        );
        builder->CreateCall(initEmptyStringCallee, ArrayRef<Value *>{
          buffStrElemPtr, 
          emptyInitStr,
          builder->getInt64(arraySize)
          }, "sprintfRes");

        // Write return value 
        const FunctionCallee writeReturnValueCallee = CL->M->getOrInsertFunction("sprintf",
          FunctionType::getInt32Ty(CL->M->getContext()),
          buffStrElemPtr->getType(),
          Type::getInt8PtrTy(CL->M->getContext()),
          cf->getReturnType()
        );
        builder->CreateCall(writeReturnValueCallee, ArrayRef<Value *>{
          buffStrElemPtr, 
          formatStr,
          CL->CI
          }, "sprintfRes");
        
        const FunctionCallee writeFunc2 = CL->M->getOrInsertFunction("write",
          FunctionType::getInt64Ty(CL->M->getContext()),
          Type::getInt32Ty(CL->M->getContext()),
          buffStrElemPtr->getType(),
          Type::getInt64Ty(CL->M->getContext())
        );

        builder->CreateCall(writeFunc2, ArrayRef<Value *>{
          fileDescriptor, 
          buffStrElemPtr,
          builder->getInt64(arraySize)
          }, "writeRetValFunction");


          // ------------------------------------------------------------
          // ---- Append Close Call -------------------------------------
          // ------------------------------------------------------------
          builder->CreateCall(CP->writeCallee, ArrayRef<Value *>{
          fileDescriptor, 
          CP->cCloseBrackStr,
          CP->cCloseBrackStrCount
          }, "writeClosingBracketFunction"
        );
        
        builder->CreateCall(
          CP->closeCallee, 
          fileDescriptor, 
          "closeRetVal"
        );
      }
  };
}

char SysCallLog::ID = 0;

static RegisterPass<SysCallLog> X("detect-syscall", "Detect-Syscall-Pass",
                            false /* Only looks at CFG */,
                            false /* Analysis Pass */);

// Automatically enable the pass.
// http://adriansampson.net/blog/clangpass.html
static void registerSysCallLogPass(const PassManagerBuilder &,
                         legacy::PassManagerBase &PM) {
  PM.add(new SysCallLog());
}

// These constructors add our pass to a list of global extensions.
static RegisterStandardPasses clangtoolLoader_Ox(PassManagerBuilder::EP_OptimizerLast, registerSysCallLogPass);
static RegisterStandardPasses clangtoolLoader_O0(PassManagerBuilder::EP_EnabledOnOptLevel0, registerSysCallLogPass);
 