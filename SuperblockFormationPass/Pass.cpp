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
        errs() << "User *U: " << *U << "\n";
        auto userInstr = dyn_cast<Instruction>(U);
        string userOpcode = userInstr -> getOpcodeName();
        if (userOpcode == "br") {
            return true;
        }
    }
    return false;
}

//Returns true if the constant variable for comparison is 0
bool isZero(Instruction &I) {
    if (I.getNumOperands() > 0) {
        Value &v = *I.getOperand(1);
        errs() << "v: " << 1 << " " << v << "\n";
        auto output = dyn_cast<Instruction>(&v);
        errs() << "v.getName()" << v.getName() << "\n";
        if (auto test = dyn_cast<ConstantInt>(&v)) {
            if (test->isZero()) {
                errs() << "is zero \n";
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
    }
    return false;
}
// Returns true if there is an i < 0 comparison
bool negativeComparison(Instruction &I, CmpInst *cmpInst) {
    if (isZero(I) && isUsedByBranch(I) && isSLT(cmpInst)) {
        return true;
    }
    return false;
}
void opcodeHeuristic(BasicBlock &BB) {
    for (Instruction &I : BB) {
        if (CmpInst *cmpInst = dyn_cast<CmpInst>(&I)) {
            errs() << "I" << I << "\n";
            negativeComparison(I, cmpInst);
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

            errs() << "header of loop: " << header << "\n";

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