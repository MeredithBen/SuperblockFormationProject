#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/Analysis/BlockFrequencyInfo.h"
#include "llvm/Analysis/BranchProbabilityInfo.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/LoopIterator.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/Support/Debug.h"
#include "llvm/Transforms/Scalar/LoopPassManager.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/LoopUtils.h"
#include "llvm/IR/Use.h"
#include "llvm/IR/User.h"
#include "llvm/IR/InstrTypes.h"

#include <iostream>

using namespace llvm;
using namespace std;
namespace {

//Get the ICmpInst from Instruction
/*
ICmpInst *ICC = dyn_cast<ICmpInst>(&I);
llvm::CmpInst::Predicate pr=ICC->getSignedPredicate();
switch(pr){
    case CmpInst::ICMP_SGT: errs()<<"------>SGT\n"; break;
    case CmpInst::ICMP_SLT: errs()<<"------>SLT\n"; break; 
    case CmpInst::ICMP_SGE: errs()<<"------>SGE\n"; break; 
    case CmpInst::ICMP_SLE: errs()<<"------>SLE\n"; break;
}
*/

//returns true if the predicate is SLT
bool isSLT(CmpInst *cmpInst) {
    if (cmpInst->getPredicate() == CmpInst::ICMP_SLT) {
        return true;
    }
    return false;
}


//Returns true if the icmp instruction is used by a branch instruction
bool isUsedByBranch(Instruction &I) {
    for (User *U : I.users()) {
        //errs() << "User *U: " << *U << "\n";
        auto userInstr = dyn_cast<Instruction>(U);
        string userOpcode = userInstr -> getOpcodeName();
        if (userOpcode == "br") {
            return true;
        }
    }
    return false;
}

//Returns true if the constant variable for comparison is 0
bool isZero(Instruction &I, Value &v) {
        auto output = dyn_cast<Instruction>(&v);
        if (auto test = dyn_cast<ConstantInt>(&v)) {
            if (test->isZero()) {
                return true;
            }
        }
        //if (isa<constantFp>(&v)) {
        //llvmconstant data
        //}
        //v --> dyncast to const int --> means it's a const int
        //int i = 0; --> getvalue to make it
        //const int --> getvalue
        //const fp class --> const floating point --> get value
    return false;
}
//Returns true if you want to take that branch (not neg comp)
//Returns false if you don't want to take that branch (neg comp)
bool isNegativeComparison (Instruction &I) {
    if (isUsedByBranch(I) && isa<ICmpInst>(&I)) {
        //errs() << "I" << I << "\n";
        ICmpInst *ICC = dyn_cast<ICmpInst>(&I);
        llvm::CmpInst::Predicate pr=ICC->getSignedPredicate();
        Value &SLT = *I.getOperand(1);
        Value &SGT = *I.getOperand(0);
        switch(pr){
            case CmpInst::ICMP_SGT: return isZero(I, SGT);
            case CmpInst::ICMP_SLT: return isZero(I, SLT);
        }
    }
    return false;
}

void opcodeHeuristic(BasicBlock &BB) {
    for (Instruction &I : BB) {
        if (isNegativeComparison(I)) {
            errs() << "I" << I << "\n";
        }
        
    }
}

struct SuperblockFormationPass : public PassInfoMixin<SuperblockFormationPass> {

    PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {
        llvm::BlockFrequencyAnalysis::Result &bfi = FAM.getResult<BlockFrequencyAnalysis>(F);
        llvm::BranchProbabilityAnalysis::Result &bpi = FAM.getResult<BranchProbabilityAnalysis>(F);
        llvm::LoopAnalysis::Result &li = FAM.getResult<LoopAnalysis>(F);

        for (Loop *L : li) {
            //This only gets each top level loop. I then need to use LoopNest class to get each nested loop. 
            BasicBlock *header = L->getHeader();
            BasicBlock *latch = L->getLoopLatch();
            BasicBlock *preheader = L->getLoopPreheader();

            //errs() << "header of loop: " << header << "\n";

        }
        for (BasicBlock &BB : F) {
            opcodeHeuristic(BB);
        }
        
        




      // Your pass is modifying the source code. Figure out which analyses are preserved and only return those, not all.
      return PreservedAnalyses::all();
    }
};
}

extern "C" ::llvm::PassPluginLibraryInfo LLVM_ATTRIBUTE_WEAK llvmGetPassPluginInfo() {
    return {
        LLVM_PLUGIN_API_VERSION, "SuperblockFormationPass", "v0.1",
        [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                ArrayRef<PassBuilder::PipelineElement>) {
                    if(Name == "superblock_pass"){
                        FPM.addPass(SuperblockFormationPass());
                        return true;
                    }
                    return false;
                }
            );
        }
    };
}