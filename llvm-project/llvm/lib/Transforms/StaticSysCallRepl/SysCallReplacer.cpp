/**
 * @author Andreas Hager, andreas.hager@aisec.fraunhofer.de
*/

#include <fcntl.h>
#include <sstream>
#include <iostream>
#include <fstream>
#include <string>
#include <unordered_set>
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

#include <nlohmann/json.hpp>

#include "SysCall.h"

using namespace llvm;
using namespace std;
using json = nlohmann::json;

namespace {
  string getDigits(string str) {
    string trucStr = "";
    for(int i = 0; i < (int)str.length(); i++) {
      if(isDigit(str[i])) {
        trucStr += str[i];
      }
    }
    return trucStr;
  }

  struct SysCallReplacer : public ModulePass {
    public:
      static char ID; // Pass identification, replacement for typeid
      SysCallReplacer() : ModulePass(ID) {
        errs() << "__INFO__: Start parsing.\n";
        string path = "/info/ReturnValues.json";
        ifstream file(path);
        string jsonObjectString = "";
        while (getline(file, jsonObjectString)) {
          // jsonObjectString contains the char '\0' in the value from the field "ReturnValue", that causes a json::pase error.
          replace(jsonObjectString.begin(), jsonObjectString.end(), '\0', '-');
          if (jsonObjectString.length() > 1 
            && jsonObjectString[0] == '{' 
            && jsonObjectString[jsonObjectString.length() - 1] == '}' 
            && json::accept(jsonObjectString)
          ) {
            auto jobj = json::parse(jsonObjectString);
            string returnValue = getDigits(jobj["ReturnValue"]);
            string sysCallID = getDigits(jobj["SysCallID"]);
            sysCalls.insert(
              *new SysCall(
                stoi(sysCallID), 
                jobj["ReturnType"], 
                stoi(returnValue)
              )
            );
          } else {
            if (
              jsonObjectString.length() > 1 && !( 
                jsonObjectString[0] == '_' 
                && jsonObjectString[jsonObjectString.length() - 1] == ':'
              )
            ) {
              errs() << "__WARNING__: Could not parse JSON: " << jsonObjectString << "\n";
            }
          }
        }
      }

      bool runOnModule(Module &M) override {
        struct CallLocation {
          Module *M;
          Function *F;
          CallInst *CI;
          const SysCall *sc;
        };
        list<CallLocation*> calls = {};

        for (Function &F : M) {
          for (BasicBlock &B : F) {
            for (Instruction &I : B) {
              CallInst *CI = dyn_cast<CallInst>(&I);
              if (CI != nullptr && isSystemCall(CI)) {
                Function *cf = CI->getCalledFunction();
                if (cf != nullptr) {
                  const SysCall *sc = getSysCall(CI);
                  if (sc != nullptr) {
                    calls.push_back(new CallLocation{ &M, &F, CI, sc });
                  } else {
                    errs() << "__WARNING__: SysCall not found:\n";
                    errs() << "\t{ \"File\": \"" << M.getName().str();
                    errs() << "\", \"Function\": \"" << F.getName().str();
                    errs() << "\", \"CalledFunc\": \"" << cf->getName().str();
                    string type_str;
                    raw_string_ostream rso(type_str);
                    cf->getReturnType()->print(rso);
                    errs() << "\", \"ReturnType\": \"" << rso.str();
                    errs() << "\" }\n";
                  }
                } else {
                  errs() << "__WARNING__: SysCall not found:\n";
                  errs() << "\t{ \"File\": \"" << M.getName().str();
                  errs() << "\", \"Function\": \"" << F.getName().str();
                  errs() << "\", \"CalledFunc\": \"" << cf->getName().str();
                  string type_str;
                  raw_string_ostream rso(type_str);
                  cf->getReturnType()->print(rso);
                  errs() << "\", \"ReturnType\": \"" << rso.str();
                  errs() << "\" }\n";
                }
              }
            }
          }
        }

        for (CallLocation *CL : calls) {
          string file = CL->M->getName().str();
          string base_filename = file.substr(file.find_last_of("/\\") + 1);
          size_t const dotIndex(base_filename.find_last_of('.'));
          string fn = base_filename.substr(0, dotIndex);
          string filename("/info/static_repl/stubbed/" + fn + "_stubbed_calls.json");
          fstream file_out;

          file_out.open(filename, ios_base::app);
          if (!file_out.is_open()) {
              errs() << "\n__ERROR__: failed to open: " << filename << '\n';
          } else {
              file_out << "{ \"File\": \"" << file;
              file_out << "\", \"Function\": \"" << CL->F->getName().str();
              file_out << "\", \"CallID\": " << getSystemCallID(CL->CI);
              Function *cf = CL->CI->getCalledFunction();
              if (cf != nullptr) {
                file_out << "\", \"CalledFunc\": \"" << cf->getName().str();
              }
              file_out << "\" }\n";
          }
          file_out.close();
          

           removeCall(CL->CI, CL->sc);
        }
        return true;
      }
      
    private:       
      unordered_set<SysCall> sysCalls;  
      vector<string> sysCallNames = {
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
            if (sysCallNames[index] == functionName) {
              return index;
            }
          }
        }
        return -1;
      }

      bool isSystemCall(CallInst *CI) {
        return getSystemCallID(CI) > 0;
      }

      const SysCall *getSysCall(CallInst *CI) {
        Function *cf = CI->getCalledFunction();
        if (cf == nullptr) return nullptr;

        string type_str;
        raw_string_ostream rso(type_str);
        cf->getReturnType()->print(rso);

        SysCall *sc = new SysCall(
          getSystemCallID(CI),
          rso.str(),
          -1
        );
        unordered_set<SysCall>::const_iterator elem = sysCalls.find(*sc);
        return elem == sysCalls.end() ? nullptr : &*elem;
      }

      void removeCall(CallInst *CI, const SysCall *sc) {
        IRBuilder<> *builder = new IRBuilder<>(CI);

        Function *cf = CI->getCalledFunction();
        if (cf == nullptr) return;
        
        Value *val;
        if (sc->returnType == "i32") {
          val = builder->getInt32(sc->returnValue);
        } else {
          val = builder->getInt64(sc->returnValue);
        }

        CI->replaceAllUsesWith(val);
        CI->eraseFromParent();
      }
  };
}

char SysCallReplacer::ID = 0;

static RegisterPass<SysCallReplacer> X("replace-syscall", "Replace-Syscall-Pass",
                            false /* Only looks at CFG */,
                            false /* Analysis Pass */);

// Automatically enable the pass.
// http://adriansampson.net/blog/clangpass.html
static void registerSysCallReplacerPass(const PassManagerBuilder &,
                         legacy::PassManagerBase &PM) {
  PM.add(new SysCallReplacer());
}

// These constructors add our pass to a list of global extensions.
static RegisterStandardPasses clangtoolLoader_Ox(PassManagerBuilder::EP_OptimizerLast, registerSysCallReplacerPass);
static RegisterStandardPasses clangtoolLoader_O0(PassManagerBuilder::EP_EnabledOnOptLevel0, registerSysCallReplacerPass);
