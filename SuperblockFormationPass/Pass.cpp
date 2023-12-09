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
#include <vector>
#include <unordered_map>
#include <queue>



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
        if (auto constInt = dyn_cast<ConstantInt>(&v)) {
            if (constInt->isZero()) {
                return true;
            }
        }
    return false;
}
bool isFloatingPt(Instruction &I) {
    if (FCmpInst *FCC = dyn_cast<FCmpInst>(&I)) {
        if (FCC->getPredicate() == CmpInst::FCMP_OEQ) {
            Value &op0 = *I.getOperand(0);
            Value &op1 = *I.getOperand(1);
            if (isa<ConstantFP>(&op0) && !(isa<ConstantFP>(&op1))) {
                //errs() << "*I.getOperand(0)" << *I.getOperand(0) << "\n";
                return true;
            }
            else if (!(isa<ConstantFP>(&op0)) && isa<ConstantFP>(&op1)){
                //errs() << "*I.getOperand(1); " << *I.getOperand(1) << "\n";
                return true;
            }
        } 
    }
    return false;
}
//Returns true if you want to take that branch (not neg comp)
//Returns false if you don't want to take that branch (neg comp)
bool isNegativeComparison (Instruction &I) {
    if (isUsedByBranch(I) && isa<ICmpInst>(&I)) {
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
            errs() << "I Not taken" << I << "\n";
        }
        else if (isFloatingPt(I)) {
            errs() << "I Not taken" << I << "\n";
        }
        
    }
}

//Gets all preds based on the current basic block
unordered_map<BasicBlock *, BasicBlock *> createPredsMap(BasicBlock &BB, llvm::LoopAnalysis::Result &li) {
    std::unordered_map<BasicBlock *, BasicBlock *> PredsMap;
    std::queue<BasicBlock *> PredsQueue;
    for (Loop *L : li) {
        BasicBlock *currBB = L->getHeader();
        BasicBlock *latch = L->getLoopLatch();
        while (currBB != latch) {
            for (BasicBlock *Pred : predecessors(currBB)) {
                PredsQueue.push(Pred);
                PredsMap[Pred] = currBB;
            }
            currBB = PredsQueue.front();
            PredsQueue.pop();
        }
    }
    return PredsMap;
    
}
//Return true if found a backwards branch
//Return false if only forwards
bool branchDirectionHeuristic(BasicBlock &BB, llvm::LoopAnalysis::Result &li) {
    std::unordered_map<BasicBlock *, BasicBlock *> PredsMap = createPredsMap(BB, li);
    for (Instruction &I : BB) {
        string opcode = I.getOpcodeName();
        if (opcode == "br") {
            for (unsigned i = 0; i < I.getNumOperands(); i++) {
                BasicBlock *opBB = dyn_cast<BasicBlock>(I.getOperand(i));
                if (PredsMap.find(opBB) != PredsMap.end()) {
                    return true;
                }
            }
        }
    }
    return false;
}

// Finds store instruction
// Gets the predecessor of that instruction
// Gets the ICMP related to that predecessor's branch instruction
// Gets the registers used in the ICMP
// Checks if the registers were loaded with the same value as the store instruction
// returns false if the same value was stored and loaded (The value was overwritten inside of the branch)
// returns true if not.
bool guardHeuristic(BasicBlock &BB) {
    for (Instruction &Istore : BB) {
        string opcode1 = Istore.getOpcodeName();
        if (opcode1 == "store" && (Istore.getNumOperands() > 1)) {
            Value &storeReg = *Istore.getOperand(1);
            for (BasicBlock *Pred : predecessors(&BB)) {
                for (Instruction &Icmp : *Pred) {
                    Value &loadVal = *Icmp.getOperand(0);
                    if (Instruction *loadInstr = dyn_cast<Instruction>(&loadVal)) {
                        string opcode2 = loadInstr->getOpcodeName();
                        if (isUsedByBranch(Icmp) && isa<ICmpInst>(&Icmp) && (opcode2 =="load") ) {
                            for (unsigned i = 0; i < loadInstr->getNumOperands(); i++) {
                                Value &loadReg = *loadInstr->getOperand(0);
                                if (&loadReg == &storeReg) {
                                    errs() << "loadReg" << loadReg << "\n";
                                    errs() << "storeReg" << storeReg << "\n";
                                    errs() << "Istore: " << Istore << "\n";
                                    errs() << "loadVal: " << loadVal << "\n";
                                    errs() << "Icmp: " << Icmp << "\n";
                                    errs() << "Pred: " << *Pred << "\n";
                                    return false;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return true;
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
        }
        for (BasicBlock &BB : F) {
            guardHeuristic(BB);

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