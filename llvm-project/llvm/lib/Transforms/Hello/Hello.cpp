//===- Hello.cpp - Example code from "Writing an LLVM Pass" ---------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements two versions of the LLVM "Hello World" pass described
// in docs/WritingAnLLVMPass.html
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Function.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/Analysis/TargetLibraryInfo.h"

#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
using namespace llvm;

#define DEBUG_TYPE "hello"

STATISTIC(HelloCounter, "Counts number of functions greeted");

namespace {
  // Hello - The first implementation, without getAnalysisUsage.
  struct Hello : public FunctionPass {
    public:
      static char ID; // Pass identification, replacement for typeid
      Hello() : FunctionPass(ID) {}

      bool runOnFunction(Function &F) override {
        ++HelloCounter;
        errs() << "Hello: ";
        errs().write_escaped(F.getName()) << '\n';
        for (BasicBlock &B : F) {
          for (Instruction &I : B) {
            if (CallInst *CB = dyn_cast<CallInst>(&I)) {
              Function *cf = CB->getCalledFunction();
              if (isSystemCall(cf)) {
                errs() << "\tF: ";
                // CB->print(errs());
                // errs() << CB->getFunction()->getName();

                errs() << cf->getName();
                errs() << ", returnType: ";
                cf->getReturnType()->print(errs(), false, false);
                errs() << '\n';
              }
            }
          }
        }
        return false;
      }

    private:
      bool isSystemCall(Function *cf) {
       
        const TargetLibraryInfo *TLI; 
        LibFunc tmp;
        return TLI->getLibFunc(cf->getName(), tmp);
       /* 
       return true;
       */
        
      }
  };
}

char Hello::ID = 0;
static RegisterPass<Hello> X("hello", "Hello World Pass",
                             false /* Only looks at CFG */,
                             false /* Analysis Pass */);

static RegisterStandardPasses Z(
    PassManagerBuilder::EP_EarlyAsPossible /*apply pass before any optimization*/,
    [](const PassManagerBuilder &Builder,
       legacy::PassManagerBase &PM) { PM.add(new Hello()); });

namespace {
  // Hello2 - The second implementation with getAnalysisUsage implemented.
  struct Hello2 : public FunctionPass {
    static char ID; // Pass identification, replacement for typeid
    Hello2() : FunctionPass(ID) {}
    bool runOnFunction(Function &F) override {
      ++HelloCounter;
      errs() << "Hello2: ";
      errs().write_escaped(F.getName()) << '\n';
      return false;
    }

    // We don't modify the program, so we preserve all analyses.
    void getAnalysisUsage(AnalysisUsage &AU) const override {
      AU.setPreservesAll();
    }
  };
}

char Hello2::ID = 0;
static RegisterPass<Hello2>
Y("hello2", "Hello World Pass (with getAnalysisUsage implemented)");
