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

struct RelBranch {
    BasicBlock *bb;
    string opcode;
    llvm::CmpInst::Predicate pr;
    std::list<std::pair<llvm::Value* , llvm::Value* >> operandPair;
    int heuristic;
    bool dir;
};

std::vector<RelBranch> relbranch;

namespace {

void relatedBranchesHeuristic(BasicBlock* BB, string opc, llvm::CmpInst::Predicate pred, std::list<std::pair<Value* , Value* >> oppair, int heur, bool path) {
    for (auto& branch : relbranch) {
        if (oppair == branch.operandPair) {
            if (heur > branch.heuristic) {
                // change the direction of the new branch according to the higher priority heuristic
                if (pred == branch.pr) {
                    relbranch.push_back({BB, opc, pred, oppair, branch.heuristic, branch.dir});
                } 
                else {
                    relbranch.push_back({BB, opc, pred, oppair, branch.heuristic, !branch.dir});
                }
                break;
            }
            else {
                // if new branch statement has a higher priority heuristic, then need to update all other branches with same variables
                branch.heuristic = heur;
                if (pred == branch.pr) {
                    branch.dir = path;
                }
                else {
                    branch.dir = !path;
                }
            }
        }
    }
    relbranch.push_back({BB, opc, pred, oppair, heur, path});
}

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
            //case CmpInst::ICMP_EQ: return isFloatingPt(I, )
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

bool isPointerEqual (Instruction &I) {
    // string userOpcode = I.getOpcodeName();
    // if (userOpcode == "br" && isa<ICmpInst>(&I)) {
    
    ICmpInst *ICC = dyn_cast<ICmpInst>(&I);
    llvm::CmpInst::Predicate pr=ICC->getSignedPredicate();
    errs() << "in pointer equal " << pr << "\n";
    Value &op2 = *I.getOperand(1);
    switch(pr){
        case CmpInst::ICMP_EQ: return true;
        break;
        case CmpInst::ICMP_NE: return false;
        break;
    }
    return false;
    // }
}

int pointerHeuristic(BasicBlock &BB) {
    for (Instruction &I : BB) {
        string userOpcode = I.getOpcodeName();
        // errs() << "in pointer, instr is" << I << "and opcode" << userOpcode << "\n";
        if (userOpcode == "icmp") {
            errs() << "the instr is " << I << "\n";
            // auto temp = dyn_cast<Instruction>(I.getOperand(0));
            auto I1 = dyn_cast<Instruction>(I.getOperand(0));
            if (isa<LoadInst>(I1)) {
                auto temp = dyn_cast<Instruction>(I1->getOperand(0));
                string tempOpcode = temp->getOpcodeName();
                if (tempOpcode == "getelementptr") {
                    auto I2 = dyn_cast<Instruction>(I.getOperand(1));
                    if (isa<LoadInst>(I2)) {
                        auto temp2 = dyn_cast<Instruction>(I2->getOperand(0));
                        string tempOpcode2 = temp2->getOpcodeName();
                        if (tempOpcode2 == "getelementptr") {
                            ICmpInst *ICC = dyn_cast<ICmpInst>(&I);
                            llvm::CmpInst::Predicate pr=ICC->getSignedPredicate();
                            llvm::Value* passop1 = I.getOperand(0);
                            llvm::Value* passop2 = I.getOperand(1);
                            std::list<std::pair<llvm::Value*, llvm::Value*>> oppair = {std::make_pair(passop1, passop2)};
                            if (isPointerEqual(I)) {
                                errs() << "Second label is taken (corresponding to else path)" << "\n";
                                relatedBranchesHeuristic(&BB, userOpcode, pr, oppair, 1, false);
                                return 2;
                            }
                            else {
                                errs() << "First label is taken (corresponding to if path)" << "\n";
                                relatedBranchesHeuristic(&BB, userOpcode, pr, oppair, 1, true);
                                return 1;
                            }
                        }
                    }
                    
                }
                else if (I.getOperand(1) == nullptr) {
                    ICmpInst *ICC = dyn_cast<ICmpInst>(&I);
                    llvm::CmpInst::Predicate pr=ICC->getSignedPredicate();
                    Value* passop1 = I.getOperand(0);
                    Value* passop2 = NULL;
                    std::list<std::pair<llvm::Value*, llvm::Value*>> oppair = {std::make_pair(passop1, passop2)};
                    switch(pr){
                        case CmpInst::ICMP_EQ: errs() << "Second label is taken (corresponding to else path)" << "\n";
                        relbranch.push_back({&BB, userOpcode, pr, oppair, 1, false});
                        return 2;
                        break;
                        case CmpInst::ICMP_NE: errs() << "First label is taken (corresponding to if path)" << "\n";
                        relbranch.push_back({&BB, userOpcode, pr, oppair, 1, true});
                        return 1;
                        break;
                        default: errs() << "pointers have some other comparison operator" << "\n";
                        return 0;
                        break;
                    }
                }
            }
            // Value* op1 = temp->getOperand(0);
            // Type* optype1 = op1->getType();
            // errs() << "here1" << *op1 << "\n";
            // if (optype1->isPointerTy()){
            //     errs() <<"here2\n";
            //     Value* op2 = I.getOperand(1);
            //     Type* optype2 = op2->getType()->getContainedType(0);
            //     if (optype2->isPointerTy()) {
            //         if (isPointerEqual(I)) {
            //             errs() << "Second label is taken (corresponding to else path)" << "\n";
            //             return 2;
            //         }
            //         else {
            //             errs() << "First label is taken (corresponding to if path)" << "\n";
            //             return 1;
            //         }
            //     }
            //     else if (op2 == nullptr) {
            //         ICmpInst *ICC = dyn_cast<ICmpInst>(&I);
            //         llvm::CmpInst::Predicate pr=ICC->getSignedPredicate();
            //         switch(pr){
            //             case CmpInst::ICMP_EQ: errs() << "Second label is taken (corresponding to else path)" << "\n";
            //             return 2;
            //             break;
            //             case CmpInst::ICMP_NE: errs() << "First label is taken (corresponding to if path)" << "\n";
            //             return 1;
            //             break;
            //             default: errs() << "pointers have some other comparison operator" << "\n";
            //             return 0;
            //             break;
            //         }
            //     }
            // }
        }        
    }
    errs() << "pointer heuristics are not used\n";
    return 0;
}

int loopHeuristic(BasicBlock &BB, llvm::LoopAnalysis::Result &li) {
    std::vector<llvm::BasicBlock *> loopHeaders;
    int flag = 0;
    for (Loop *L : li) {
        BasicBlock *header = L->getHeader();
        errs() << "loop header is " << *(header->getTerminator()->getSuccessor(0)) << "\n";
        loopHeaders.push_back(header->getTerminator()->getSuccessor(0));
    }
    for (Instruction &I : BB) {
        string userOpcode = I.getOpcodeName();
        if (userOpcode == "br") {
            for (BasicBlock *Succ: successors(&BB)) {
                auto headercheck = std::find(loopHeaders.begin(), loopHeaders.end(), Succ);
                if (headercheck != loopHeaders.end()) {
                    errs() << "This block is taken" << *Succ << "\n";
                    flag = 1;
                    llvm::CmpInst::Predicate pr;
                    Value* passop1 = NULL;
                    Value* passop2 = NULL;
                    std::list<std::pair<llvm::Value*, llvm::Value*>> oppair = {std::make_pair(passop1, passop2)};
                    relbranch.push_back({&BB, userOpcode, pr, oppair, 2, true});
                    return 1;
                    // store(instruction/bb, predicate, opcode, variables, heuristic), could be a global var, or local passed from main func
                }
            }
            
        }
    }
    if (flag == 0) {
        errs() << "loop heuristics not applied" << "\n";
        return 0;
    }
    return 0;
}

void runHeuristics(Function &F, llvm::LoopAnalysis::Result &li) {
    for (BasicBlock &BB : F) {
        pointerHeuristic(BB);
        loopHeuristic(BB, li);
        opcodeHeuristic(BB);
    }
    errs() << relbranch[0].heuristic << "\n";
        
}

BasicBlock* getMostLikely(BasicBlock* curr) {
    for (auto& branch : relbranch) {
        if (curr == branch.bb) {
            bool path = branch.dir;
            if (path) {
                return curr->getTerminator()->getSuccessor(0);
            }
            else {
                return curr->getTerminator()->getSuccessor(1);
            }
        }
    }
    errs() << "couldn't find a successor, something wrong\n";
    exit(0);
}

double getAccuracy(Function &F, llvm::BranchProbabilityAnalysis::Result &bpi, llvm::LoopAnalysis::Result &li) {
    std::vector<llvm::BasicBlock *> freqPath;
    for (Loop *L : li) {
        BasicBlock *header = L->getHeader();
        BasicBlock *latch = L->getLoopLatch();

        BasicBlock *currblock = header;

        while(currblock != latch){
            freqPath.push_back(currblock);
            for (BasicBlock *Succ: successors(currblock)) {
                BranchProbability prob = bpi.getEdgeProbability(currblock, Succ);
                uint64_t num = prob.getNumerator();
                uint64_t den = prob.getDenominator();
                double ratio = num/ static_cast<double>(den);
                double comp = static_cast<double>(0.7999);
                if (ratio >= comp) {
                    currblock = Succ; //move to the next block on the frequent path
                    break;
                }
            }
        }
        freqPath.push_back(latch);
    }

    BasicBlock* pred = nullptr;
    double sa_acc = 0, pr_acc = 0;
    for (auto BB : freqPath) {
        
        if (pred != nullptr) {
            
            Instruction* inst = pred->getTerminator();
            string opc = inst->getOpcodeName();
            BranchProbability prob = bpi.getEdgeProbability(pred, BB);
            uint64_t num = prob.getNumerator();
            uint64_t den = prob.getDenominator();
            double ratio = num/ static_cast<double>(den);
            pr_acc = pr_acc + ratio;
            if (ratio == 1) {
                sa_acc = sa_acc + ratio;
            }
            else {
                BasicBlock* succ = getMostLikely(pred);
                if (succ == BB) {
                    sa_acc = sa_acc + ratio;
                }
                else {
                    BranchProbability prob1 = bpi.getEdgeProbability(pred, succ);
                    uint64_t num1 = prob.getNumerator();
                    uint64_t den1 = prob.getDenominator();
                    double ratio1 = num1/ static_cast<double>(den1);
                    sa_acc = sa_acc + ratio1;
                }
            }
            errs() << "here ratio" << ratio << "   " << *BB << "\n";
        }
        
        pred = BB;

    }
    return sa_acc/pr_acc;
}

struct SuperblockFormationPass : public PassInfoMixin<SuperblockFormationPass> {

    PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {
        llvm::BlockFrequencyAnalysis::Result &bfi = FAM.getResult<BlockFrequencyAnalysis>(F);
        llvm::BranchProbabilityAnalysis::Result &bpi = FAM.getResult<BranchProbabilityAnalysis>(F);
        llvm::LoopAnalysis::Result &li = FAM.getResult<LoopAnalysis>(F);

        runHeuristics(F, li);
        double acc = getAccuracy(F, bpi, li);




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