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
  struct SysCallMod : public ModulePass {
    public:
      static char ID; // Pass identification, replacement for typeid
      SysCallMod() : ModulePass(ID) {}
      

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
        const FunctionCallee containsReturnValueCallee;

        /* i64 CALLEE'S */
        const FunctionCallee addReturnValuei64Callee;
        const FunctionCallee getReturnValuei64Callee;

        /* i32 CALLEE'S */
        const FunctionCallee addReturnValuei32Callee;
        const FunctionCallee getReturnValuei32Callee;
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
        
        if (cfRetrunTypeString == "void") {
          CL->CI->eraseFromParent();
          errs() << "\n__INFO__: Erased call: \n";
          errs() << "\t{ \"File\": \"" << file;
          errs() << "\", \"Function\": \"" << CL->F->getName().str();
          errs() << "\", \"CalledFunc\": \"" << cf->getName().str();
          errs() << "\", \"ReturnType\": \"" << cfRetrunTypeString;
          errs() << "\" }\n";
          return;
        }

        const bool isI32 = cfRetrunTypeString == "i32";
        if (!isI32 && cfRetrunTypeString != "i64") {
          errs() << "\n__ERROR__: Abort call modification for: \n";
          errs() << "\t{ \"File\": \"" << file;
          errs() << "\", \"Function\": \"" << CL->F->getName().str();
          errs() << "\", \"CalledFunc\": \"" << cf->getName().str();
          errs() << "\", \"ReturnType\": \"" << cfRetrunTypeString;
          errs() << "\" }\n";
          return;
        }

        string base_filename = file.substr(file.find_last_of("/\\") + 1);
        size_t const dotIndex(base_filename.find_last_of('.'));
        string fn = base_filename.substr(0, dotIndex);
        string filename("/info/dynamic/stubbed/" + fn + "_stubbed_calls.json");
        fstream file_out;

        file_out.open(filename, ios_base::app);
        if (!file_out.is_open()) {
            errs() << "\n__ERROR__: failed to open: " << filename << '\n';
        } else {
            file_out << "{ \"File\": \"" << file;
            file_out << "\", \"Function\": \"" << CL->F->getName().str();
            file_out << "\", \"CallID\": " << getSystemCallID(CL->CI);
            file_out << "\", \"CalledFunc\": \"" << cf->getName().str();
            file_out << "\", \"ReturnType\": \"" << cfRetrunTypeString;
            file_out << "\" }\n";
        }

        file_out.close();
        
        const int sysCallID = getSystemCallID(CL->CI);

        Instruction *InsertBefore = CL->CI->getNextNode();
        
        Function *function = InsertBefore->getParent()->getParent();
        
        BasicBlock *Head_bb = InsertBefore->getParent();
        BasicBlock *Tail_bb = Head_bb->splitBasicBlock(InsertBefore, "tail_bb");
        BasicBlock *elseAddRV_bb = BasicBlock::Create(
          InsertBefore->getContext(), 
          "add_return_value", function,
          Tail_bb
        );
        BasicBlock *thenGetRV_bb = BasicBlock::Create(
          InsertBefore->getContext(), 
          "get_return_value", function,
          elseAddRV_bb
        );

        IRBuilder<> *builder = new IRBuilder<>(Head_bb->getTerminator());
        
        // --------------
        // ---- HEAD ----
        // --------------
        Value *containsRetVal;
        containsRetVal = builder->CreateCall(
          CP->containsReturnValueCallee, 
          ArrayRef<Value *>{
            builder->getInt32(sysCallID)
          }, "containsRetVal"
        );
        
        Value *containsRVCond = builder->CreateICmpSGT(
          containsRetVal, 
          builder->getInt32(0)
        );

        BranchInst *containsRVCondBr = BranchInst::Create(
          thenGetRV_bb, 
          elseAddRV_bb,
          containsRVCond
        );
        ReplaceInstWithInst(Head_bb->getTerminator(), containsRVCondBr);

        // --------------
        // ---- THEN ----
        // --------------
        // 2. then get return value from store
        builder->SetInsertPoint(thenGetRV_bb);

        Value *rvValue;
        if (isI32) {
          rvValue = builder->CreateCall(
            CP->getReturnValuei32Callee, 
            ArrayRef<Value *>{
              builder->getInt32(sysCallID)
            }, "getReturnValuei32"
          );
        } else {
          rvValue = builder->CreateCall(
            CP->getReturnValuei64Callee, 
            ArrayRef<Value *>{
              builder->getInt32(sysCallID)
            }, "getReturnValuei64"
          );
        }
        
        
        builder->CreateBr(Tail_bb);
        // Codegen of thenGetRV_bb can change the current block, update thenGetRV_bb for the PHI.
        thenGetRV_bb = builder->GetInsertBlock();

        // --------------
        // ---- ELSE ----
        // --------------
        // 3. else execute syscall and add return value to store 
        builder->SetInsertPoint(elseAddRV_bb);

        Instruction *returnValueSysCall = CL->CI->clone();
        builder->Insert(returnValueSysCall);
        if (isI32) {
          builder->CreateCall(
            CP->addReturnValuei32Callee, 
            ArrayRef<Value *>{
              builder->getInt32(sysCallID),
              returnValueSysCall
            }
          );
        } else {
          builder->CreateCall(
            CP->addReturnValuei64Callee, 
            ArrayRef<Value *>{
              builder->getInt32(sysCallID),
              returnValueSysCall
            }
          );
        }

        builder->CreateBr(Tail_bb);
        // Codegen of elseAddRV_bb can change the current block, update elseAddRV_bb for the PHI.
        elseAddRV_bb = builder->GetInsertBlock();

        // --------------
        // ---- TAIL ----
        // --------------
        builder->SetInsertPoint(Tail_bb->getFirstNonPHI());
        PHINode *phi = builder->CreatePHI(
          returnValueSysCall->getType(),
          2, 
          "retVal"
        );
        phi->addIncoming(rvValue, thenGetRV_bb);
        phi->addIncoming(returnValueSysCall, elseAddRV_bb);
        
        CL->CI->replaceAllUsesWith(phi);
        CL->CI->eraseFromParent();
      }

      Components *init(Module *M) {
        // ------------------------------------------------
        // ---- containsReturnValue Callee Preparation ----
        // ------------------------------------------------
        const FunctionCallee containsReturnValueCallee = M->getOrInsertFunction("containsReturnValue",
          FunctionType::getInt32Ty(M->getContext()),
          Type::getInt32Ty(M->getContext())
        );

        // --------------------------------
        // ---- i64 Callee Preparation ----
        // --------------------------------
        const FunctionCallee addReturnValuei64Callee = M->getOrInsertFunction("addReturnValuei32",
          FunctionType::getVoidTy(M->getContext()),
          Type::getInt32Ty(M->getContext()),
          Type::getInt64Ty(M->getContext())
        );
        const FunctionCallee getReturnValuei64Callee = M->getOrInsertFunction("getReturnValuei32",
          FunctionType::getInt64Ty(M->getContext()),
          Type::getInt32Ty(M->getContext())
        );

        // --------------------------------
        // ---- i32 Callee Preparation ----
        // --------------------------------
        const FunctionCallee addReturnValuei32Callee = M->getOrInsertFunction("addReturnValuei64",
          FunctionType::getVoidTy(M->getContext()),
          Type::getInt32Ty(M->getContext()),
          Type::getInt32Ty(M->getContext())
        );
        const FunctionCallee getReturnValuei32Callee = M->getOrInsertFunction("getReturnValuei64",
          FunctionType::getInt32Ty(M->getContext()),
          Type::getInt32Ty(M->getContext())
        );

        return new Components {
          containsReturnValueCallee,

          addReturnValuei64Callee,
          getReturnValuei64Callee,

          addReturnValuei32Callee,
          getReturnValuei32Callee
        };
      }
  };
}

char SysCallMod::ID = 0;

static RegisterPass<SysCallMod> X("detect-syscall", "Detect-Syscall-Pass",
                            false /* Only looks at CFG */,
                            false /* Analysis Pass */);

// Automatically enable the pass.
// http://adriansampson.net/blog/clangpass.html
static void registerSysCallModPass(const PassManagerBuilder &,
                         legacy::PassManagerBase &PM) {
  PM.add(new SysCallMod());
}

// These constructors add our pass to a list of global extensions.
static RegisterStandardPasses clangtoolLoader_Ox(PassManagerBuilder::EP_OptimizerLast, registerSysCallModPass);
static RegisterStandardPasses clangtoolLoader_O0(PassManagerBuilder::EP_EnabledOnOptLevel0, registerSysCallModPass);
