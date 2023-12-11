#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/Analysis/BlockFrequencyInfo.h"
#include "llvm/Analysis/BranchProbabilityInfo.h"
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

#include "llvm/Analysis/LoopNestAnalysis.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/Trace.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/ValueMapper.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"

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
        if (isUsedByBranch(I) && isa<ICmpInst>(&I)) {
            if (isNegativeComparison(I)) {
                string userOpcode = I.getOpcodeName();
                ICmpInst *ICC = dyn_cast<ICmpInst>(&I);
                llvm::CmpInst::Predicate pr=ICC->getSignedPredicate();
                llvm::Value* passop1 = I.getOperand(0);
                llvm::Value* passop2 = I.getOperand(1);
                std::list<std::pair<llvm::Value*, llvm::Value*>> oppair = {std::make_pair(passop1, passop2)};
                errs() << "I Not taken" << I << "\n";
                relatedBranchesHeuristic(&BB, userOpcode, pr, oppair, 3, false);
            }
            else {
                string userOpcode = I.getOpcodeName();
                ICmpInst *ICC = dyn_cast<ICmpInst>(&I);
                llvm::CmpInst::Predicate pr=ICC->getSignedPredicate();
                llvm::Value* passop1 = I.getOperand(0);
                llvm::Value* passop2 = I.getOperand(1);
                std::list<std::pair<llvm::Value*, llvm::Value*>> oppair = {std::make_pair(passop1, passop2)};
                relatedBranchesHeuristic(&BB, userOpcode, pr, oppair, 3, true);
            }
        }
        else if (FCmpInst *FCC = dyn_cast<FCmpInst>(&I)) {
            if (isFloatingPt(I)) {
                errs() << "I Not taken" << I << "\n";
                
                string userOpcode = I.getOpcodeName();
                
                FCmpInst *FCC = dyn_cast<FCmpInst>(&I);
                llvm::CmpInst::Predicate pr=FCC->getSignedPredicate();
                
                llvm::Value* passop1 = I.getOperand(0);
                
                llvm::Value* passop2 = I.getOperand(1);
                std::list<std::pair<llvm::Value*, llvm::Value*>> oppair = {std::make_pair(passop1, passop2)};
                relatedBranchesHeuristic(&BB, userOpcode, pr, oppair, 3, false);
            }
            else {
                string userOpcode = I.getOpcodeName();
                ICmpInst *ICC = dyn_cast<ICmpInst>(&I);
                llvm::CmpInst::Predicate pr=ICC->getSignedPredicate();
                llvm::Value* passop1 = I.getOperand(0);
                llvm::Value* passop2 = I.getOperand(1);
                std::list<std::pair<llvm::Value*, llvm::Value*>> oppair = {std::make_pair(passop1, passop2)};
                relatedBranchesHeuristic(&BB, userOpcode, pr, oppair, 3, true);
            }
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

bool branchDirectionHeuristic(BasicBlock &BB, llvm::LoopAnalysis::Result &li) {
    std::unordered_map<BasicBlock *, BasicBlock *> PredsMap = createPredsMap(BB, li);
    for (Instruction &I : BB) {
        string opcode = I.getOpcodeName();
        if (opcode == "br") {
            for (unsigned i = 0; i < I.getNumOperands(); i++) {
                BasicBlock *opBB = dyn_cast<BasicBlock>(I.getOperand(i));
                if (PredsMap.find(opBB) != PredsMap.end()) {
                    ICmpInst *ICC = dyn_cast<ICmpInst>(&I);
                    llvm::CmpInst::Predicate pr;
                    Value* passop1 = NULL;
                    Value* passop2 = NULL;
                    std::list<std::pair<llvm::Value*, llvm::Value*>> oppair = {std::make_pair(passop1, passop2)};
                    relatedBranchesHeuristic(&BB, opcode, pr, oppair, 5, true);
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
        string opcode3 = Istore.getOpcodeName();
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
                                    if(opcode3 == "icmp" && isUsedByBranch(Istore)) {
                                        string userOpcode = opcode3;
                                        ICmpInst *ICC = dyn_cast<ICmpInst>(&Istore);
                                        llvm::CmpInst::Predicate pr=ICC->getSignedPredicate();
                                        llvm::Value* passop1 = Istore.getOperand(0);
                                        llvm::Value* passop2 = Istore.getOperand(1);
                                        std::list<std::pair<llvm::Value*, llvm::Value*>> oppair = {std::make_pair(passop1, passop2)};
                                        relatedBranchesHeuristic(&BB, userOpcode, pr, oppair, 4, true);
                                    }
                                    
                                    return true;
                                }
                                else {
                                    if(opcode3 == "icmp" && isUsedByBranch(Istore)) {
                                        string userOpcode = opcode3;
                                        ICmpInst *ICC = dyn_cast<ICmpInst>(&Istore);
                                        llvm::CmpInst::Predicate pr=ICC->getSignedPredicate();
                                        llvm::Value* passop1 = Istore.getOperand(0);
                                        llvm::Value* passop2 = Istore.getOperand(1);
                                        std::list<std::pair<llvm::Value*, llvm::Value*>> oppair = {std::make_pair(passop1, passop2)};
                                        relatedBranchesHeuristic(&BB, userOpcode, pr, oppair, 4, false);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return false;
}

BasicBlock * nextBB(BasicBlock &BB, bool isFirstBranch) {
    errs() << "BB: " << BB<< "\n";
    for (Instruction &I: BB) {
        string opcode = I.getOpcodeName();
        if (opcode == "br") {
            if (I.getNumOperands() == 1) {
                BasicBlock *returnBB = dyn_cast<BasicBlock>((I.getOperand(0)));
                BasicBlock &refBB = *returnBB;
                errs() << "refBB0:" << refBB << "\n";
                return returnBB;
            }
            else if (I.getNumOperands() > 1 && (isFirstBranch)) {
                BasicBlock *returnBB = dyn_cast<BasicBlock>((I.getOperand(1)));
                BasicBlock &refBB = *returnBB;
                errs() << "refBB1:" << refBB << "\n";
                return returnBB;
            }
            else {
                BasicBlock *returnBB = dyn_cast<BasicBlock>((I.getOperand(2)));
                BasicBlock &refBB = *returnBB;
                errs() << "refBB2:" << refBB << "\n";
                return returnBB;
            }
        }
    }
    return &BB;
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
            if (auto I1 = dyn_cast<Instruction>(I.getOperand(0))) {
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
            }
            
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
        errs() << "in BB " << BB << "\n";
        pointerHeuristic(BB);
        loopHeuristic(BB, li);
        opcodeHeuristic(BB);
        guardHeuristic(BB);
        branchDirectionHeuristic(BB, li);

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

double getAccuracy(Function &F, llvm::BranchProbabilityAnalysis::Result &bpi, llvm::LoopAnalysis::Result &li){
    double stsum = 0;
    double prsum = 0;
    for (BasicBlock &BB : F) {
        int count = 0;
        for (BasicBlock *Succ: successors(&BB)) {
            count += 1;
        }
        if (count > 1) {
            BasicBlock* succ1 = BB.getTerminator()->getSuccessor(0);
            BasicBlock* succ2 = BB.getTerminator()->getSuccessor(1);
            BranchProbability prob1 = bpi.getEdgeProbability(&BB, succ1);
            BranchProbability prob2 = bpi.getEdgeProbability(&BB, succ2);
            uint64_t num1 = prob1.getNumerator();
            uint64_t den1 = prob1.getDenominator();
            double ratio1 = num1/ static_cast<double>(den1);
            uint64_t num2 = prob2.getNumerator();
            uint64_t den2 = prob2.getDenominator();
            double ratio2 = num2/ static_cast<double>(den2);
            if (ratio1 > ratio2) {
                prsum += ratio1;
            }
            else {
                prsum += ratio2;
            }
            BasicBlock* mostLikely = getMostLikely(&BB);
            if (mostLikely == succ1) {
                stsum += ratio1;
            }
            else{
                stsum += ratio2;
            }
        }
    }
    return stsum/prsum;
}

// --------------------------------------- the growTrace function -------------------------------------------------------
std::list<BasicBlock*> visited;

Trace growTrace(BasicBlock* current_block, DominatorTree& dom_tree){
    //initialize trace with current_block
    std::vector<BasicBlock*> trace_blocks;
    trace_blocks.push_back(current_block);

    //trace out the optimal path through loop according to hazard-avoidance and heuristics
    while(1){
        visited.push_back(current_block);
        //check if current block contains a subroutine return or indirect jump
        std::string opcodeName;
        for(Instruction &I : *current_block){
            opcodeName = I.getOpcodeName();
            if(opcodeName == "ret"){
                errs() << "Found a subroutine return!" << "\n";
                Trace temp_trace = Trace(trace_blocks);
                return temp_trace; // stop growing the trace
            }
            if(opcodeName == "indirectbr"){
                errs() << "Found an indirect jump!" << "\n";
                Trace temp_trace = Trace(trace_blocks);
                return temp_trace; //stop growing trace
            }
        }
        //get the likely block
        BasicBlock* likely_block;
        if(current_block->getSingleSuccessor()){
            //then add the successor to the trace and make succ the new current block
            likely_block = current_block->getSingleSuccessor();
        }else{
            //then call the heuristics function and pass the current block to it, receive the optimal successor in return
            likely_block = getMostLikely(current_block); //needs to account for coming out of the loop -- loop heuristic should do that.
        }
        //check if likely_block has been visited, and if not, add it to the trace
        if (std::find(visited.begin(), visited.end(), likely_block) == visited.end()){
            // the likely_block has not been visited
            if(dom_tree.dominates(likely_block, current_block)){
                errs() << "The likely block dominates the current block! Stop! \n";
                Trace temp_trace = Trace(trace_blocks);
                return temp_trace;
            }
            
            //then likely does not dominate current
            errs() << "The likely block does not dominate the current block.\n";
            trace_blocks.push_back(likely_block);
            current_block = likely_block;
        }else{
            Trace temp_trace = Trace(trace_blocks);
            return temp_trace;
        }
    }
}
        
//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% start of pass %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
struct SuperblockFormationPass : public PassInfoMixin<SuperblockFormationPass> {

    PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {
        // llvm::BlockFrequencyAnalysis::Result &bfi = FAM.getResult<BlockFrequencyAnalysis>(F);
        llvm::BranchProbabilityAnalysis::Result &bpi = FAM.getResult<BranchProbabilityAnalysis>(F);
        llvm::LoopAnalysis::Result &li = FAM.getResult<LoopAnalysis>(F);
        DominatorTree dt = DominatorTree(F);

        runHeuristics(F, li);
     
        std::list<Trace> traces;
        // ------------------------------------------ identifying loops ---------------------------------------------------------
        // set up the lists and initialize them with top level loops in program
        std::list<Loop*> bfs_loops;
        std::list<Loop*> least_to_most_nested;
        for (Loop* L : li) {
            errs() << "top level loop: " << *L << "\n";
            bfs_loops.push_back(L);
            least_to_most_nested.push_back(L);
        }

        // now go through each loop in program in breadth-first order and add them to least_to_most_list
        Loop* current_loop;
        while (!bfs_loops.empty()){
            // pop the top of the queue and set that to be current loop (initialize stack with all top level loops from LoopInfo)
            current_loop = bfs_loops.front();
            bfs_loops.pop_front();
            // for each top level loop in the loop you are currently visiting, add the loop object to a stack.
                // also add the loop to a list to be used later
            for (Loop *SL : current_loop->getSubLoops()) {
                errs() << "subloop: " << *SL << "\n";
                bfs_loops.push_back(SL);
                least_to_most_nested.push_back(SL);
            }
        }

        //sanity check: print out loop depths to check they were ordered correctly. 
        for (Loop* temp_loop : least_to_most_nested){
            errs() << "Loop depth: " << temp_loop->getLoopDepth() << "\n";
        }

        // -------------------------------------- trace formation: loop bodies --------------------------------------------------
        
        //form traces with the loop bodies first
        while(!least_to_most_nested.empty()){
            //to iterate through the loops in most-nested order, access the back element of list and then pop it off.
            Loop* current_loop = least_to_most_nested.back();
            least_to_most_nested.pop_back();

            //create bfs list of loop blocks, following same concept as above for bfs loops
            std::list<BasicBlock*> bfs_blocks;  
            std::list<BasicBlock*> loop_blocks;  
            std::vector<BasicBlock*> blocks_vector = current_loop->getBlocksVector();
            BasicBlock* header = current_loop->getHeader();
            loop_blocks.push_back(header);
            bfs_blocks.push_back(header);

            errs() << "Starting new list of loop blocks! -------------- \n";
        
            BasicBlock* current_loop_block;
            while(!loop_blocks.empty()){
                current_loop_block = loop_blocks.front();
                loop_blocks.pop_front();

                for(BasicBlock* succ : successors(current_loop_block)){
                    //if the successor is the header of the loop, we have taken backedge and need to stop
                    if(succ == header){
                        break;
                    }
                    //if the successor has already been added to bfs_blocks, don't add it again (happens when taking backedges that aren't from latch of current loop)
                    if(std::find(bfs_blocks.begin(), bfs_blocks.end(), succ) != bfs_blocks.end()){
                        break;
                    }
                    //if the successor is in the loop, add it to the bfs_loops list
                    if(std::find(blocks_vector.begin(), blocks_vector.end(), succ) != blocks_vector.end()){
                        loop_blocks.push_back(succ);
                        bfs_blocks.push_back(succ);
                    }
                }
            }
            
            // //sanity check: print out basic blocks to check they were ordered correctly. 
            // for (BasicBlock* temp_block : bfs_blocks){
            //     errs() << "Basic Block: " << *temp_block << "\n";
            // }

            // iterate through blocks in loop and forrm traces
            for(BasicBlock* current_block : bfs_blocks){
                if (std::find(visited.begin(), visited.end(), current_block) == visited.end()){
                    // the current_block has not been visited
                    Trace temp_trace = growTrace(current_block, dt);
                    traces.push_back(temp_trace);
                    errs() << "New trace --------------------------------------------- \n";
                    for(BasicBlock* bb : temp_trace){
                        errs() << "Trace bb: " << *bb << "\n";
                    }
                }
            }
        }
        // ---------------------------------------- trace formation: function blocks --------------------------------------------
        BasicBlock& entry_block_addr = F.getEntryBlock();
        BasicBlock* entry_block = &entry_block_addr;
        std::list<BasicBlock*> function_blocks;
        std::list<BasicBlock*> bfs_function_blocks;
       
        function_blocks.push_back(entry_block);
        bfs_function_blocks.push_back(entry_block);

        while(!function_blocks.empty()){
            BasicBlock* current_function_block = function_blocks.front();
            function_blocks.pop_front();

            for(BasicBlock* succ : successors(current_function_block)){
                //if the successor has already been added to bfs_blocks, don't add it again (happens when taking backedges that aren't from latch of current loop)
                if(std::find(bfs_function_blocks.begin(), bfs_function_blocks.end(), succ) != bfs_function_blocks.end()){
                    continue;
                }else{
                    function_blocks.push_back(succ);
                    bfs_function_blocks.push_back(succ);
                }
            }
        }
        // //sanity check: print out basic blocks to check they were ordered correctly. 
        // for (BasicBlock* temp_block : bfs_function_blocks){
        //     errs() << "Basic Block: " << *temp_block << "\n";
        // }

        //now do trace formation for remaining function blocks
        for(BasicBlock* current_block : bfs_function_blocks){
            if (std::find(visited.begin(), visited.end(), current_block) == visited.end()){
                // the current_block has not been visited
                Trace temp_trace = growTrace(current_block, dt);
                traces.push_back(temp_trace);
                errs() << "New trace --------------------------------------------- \n";
                for(BasicBlock* bb : temp_trace){
                    errs() << "Trace bb: " << *bb << "\n";
                }
            }
        }
        // ----------------------------------------------- tail duplication -----------------------------------------------------
        //if there is a block in the trace other than the header that has multiple predecessors, we need to tail duplicate that block and all remaining blocks in trace below it
        std::vector<Value*> usesToReplace;
        std::vector<Instruction*> phisToReplaceWith;
        ValueToValueMapTy VMap;

        std::vector<std::vector<BasicBlock*>> list_of_bb_to_clone_lists;
        std::vector<std::vector<BasicBlock*>> list_of_tail_lists;
        for(Trace curr_trace : traces){
            BasicBlock* first_in_trace = curr_trace.getEntryBasicBlock();
            for(BasicBlock* curr_bb : curr_trace){
                if(curr_bb->hasNPredecessorsOrMore(2) && curr_bb != first_in_trace){
                    //if there is a block in the trace that has 2 or more predecessors, and it isn't the header, need to tail-duplicate
            
                    auto trace_size = curr_trace.size();
                    auto curr_index = curr_trace.getBlockIndex(curr_bb);
                    errs() << "The length of the trace is: " << trace_size << " and the index is "<< curr_index <<"\n";
                    
                    std::vector<BasicBlock*> bb_to_clone_list;
                    std::vector<BasicBlock*> tail_list;
                    std::list<BasicBlock*> cloned_blocks; //create a stack of cloned blocks in trace, pushing and popping from back
                    for(int i=curr_index; i<trace_size; i++){ //for all of the blocks in the trace after the side entrance
                        BasicBlock* bb_to_clone = curr_trace.getBlock(i); 
                        
                        //check if the BB has multiple predecessors but they are all in the trace
                        bool needToClone = false;
                        for(BasicBlock* parent : predecessors(bb_to_clone)){
                            if(std::find(curr_trace.begin(), curr_trace.end(), parent) == curr_trace.end()){
                                needToClone = true; //if there is a parent that is not in the trace, we need to clone
                                errs() << "We need to clone: " << *bb_to_clone << "\n";
                            }
                        }
                        if(needToClone){
                            BasicBlock* cloned_bb = CloneBasicBlock(bb_to_clone, VMap);
                            cloned_bb->insertInto(&F); //insert the cloned_bb into the function
                            tail_list.push_back(cloned_bb);
                            bb_to_clone_list.push_back(bb_to_clone);
                            //errs() << "The cloned bb is: " << *cloned_bb <<"\n";
                            
                            //need to change the predecessors of the bb_to_clone and the cloned_bb
                            for(BasicBlock* pred : predecessors(bb_to_clone)){ 
                                //if the basic block only has one predecessor, then it is the second/third/etc in the trace
                                if(!bb_to_clone->hasNPredecessorsOrMore(2)){ 
                                    //need to connect cloned_bb as a successor of the previously cloned block
                                    BasicBlock* latest_clone = cloned_blocks.back();
                                    cloned_blocks.pop_back();
                                    Instruction* terminator = latest_clone->getTerminator();
                                    terminator->replaceSuccessorWith(bb_to_clone, cloned_bb);
                                    cloned_blocks.push_back(cloned_bb);
                                    errs() << "The cloned bb (only one pred) is now: " << *cloned_bb << "\n";
                                }
                                //if bb has more than one predecssor, one pred is in trace and should stay connected to bb_to_clone
                                    //but other pred not in trace and should renove connection to curr_bb and instead connect to cloned_bb
                                else if(std::find(curr_trace.begin(), curr_trace.end(), pred) == curr_trace.end()){
                                    errs() << "We need to clone this multi pred block! " << *bb_to_clone << "\n";
                                    Instruction* terminator = pred->getTerminator();
                                    terminator->replaceSuccessorWith(bb_to_clone, cloned_bb);
                                    cloned_blocks.push_back(cloned_bb);
                                    errs() << "The cloned bb is now: " << *cloned_bb << "\n";
                                }
                            }
                        }
                        
                        //after cloning a basic block, need to fix up by adding phi nodes to the join point
                        //for every variable that is assigned in the bb_to_clone, need to add a phi in join point with vals from bb_to_clone and cloned_bb
                        
                        //the join point will be all of the the immediate successors of the last block that was pushed back into cloned_blocks
                        // BasicBlock* last_cloned = cloned_blocks.back();
                        // for(BasicBlock* join_point : successors(last_cloned)){
                        
                        //     errs() << "The join point is: " << *join_point << "\n";
                        //     for(Instruction& bb_inst : *bb_to_clone){
                        //         if(!bb_inst.getType()->isVoidTy()){
                        //             //if the instruction returns something, then it must added to a phi_node at join point
                        //             //then get the matching instruction from the cloned block
                        //             for(Instruction& clone_inst : *cloned_bb){
                        //                 if(clone_inst.isIdenticalTo(&bb_inst)){ //then we have our two instructions to use in phi node!
                        //                     //cast instructions to values
                        //                     Value* bb_inst_val = dyn_cast<Value>(&bb_inst);
                        //                     Value* clone_inst_val = dyn_cast<Value>(&clone_inst);

                        //                     //create phi node and insert it at join point
                        //                     PHINode *phi = PHINode::Create(bb_inst_val->getType(), 0, Twine("phiNode"));
                        //                     phi->insertBefore(join_point->getFirstNonPHI()); 

                        //                     //add the two values along with their BBs to the phi node
                        //                     phi->addIncoming(bb_inst_val, bb_to_clone);
                        //                     phi->addIncoming(clone_inst_val, cloned_bb);
            
                        //                     //add instructions and phi nodes to vectors to later replace uses
                        //                     usesToReplace.push_back(bb_inst_val);
                        //                     phisToReplaceWith.push_back(phi);
                        //                 }
                        //             }
                        //         }
                        //     }

                        // }
                    }
                    list_of_tail_lists.push_back(tail_list);
                    list_of_bb_to_clone_lists.push_back(bb_to_clone_list);
                }
            }
        }
        // for (int i=0; i<usesToReplace.size(); i++){
        //     Value* use = usesToReplace[i];
        //     Instruction* phi = phisToReplaceWith[i];
        //     Value* phi_val = dyn_cast<Value>(phi);
        //     BasicBlock* join_point = phi->getParent();


        //     PHINode* phin = dyn_cast<PHINode>(phi);
        //     errs() << "The phi val is complete: " << phin->isComplete() << "\n";
        //     if(!phin->isComplete()){
    
        //         for(BasicBlock* par : predecessors(join_point)){
        //             //Value* temp_val = phin->getIncomingValueForBlock(par);
        //             for(int i=0; i<4; i++){
        //                 int idx = phin->getBasicBlockIndex(par);
        //                 if(idx == -1){ //if the parent basic block isn't in the phi, add incoming w/ dummy value
        //                     // Type ty = phi_val->getType();
        //                     // PointerType* pt_ty = PointerType::get(&ty);
        //                     // ConstantPointerNull* v = ConstantPointerNull::get(pt_ty);
        //                     Value* v = NULL; //can't assign a null value here
        //                     phin->addIncoming(v, par);
        //                     errs() << "Phi node is now " << *phin << "\n";
        //                 }
        //             }
        //         }
        //     }
            
        //     use->replaceUsesOutsideBlock(phi_val, join_point);
        // }

        // -------------------------------------- fixing up uses in duplicated tail ---------------------------------------------
        std::vector<User*> user_list;
        std::vector<Value*> bb_inst_val_list;
        std::vector<Value*> clone_inst_val_list;

        for(int i=0; i<list_of_tail_lists.size(); i++){
            errs() << "Start of new duplicated tail. \n";
            std::vector<BasicBlock*> tail_list = list_of_tail_lists[i];
            std::vector<BasicBlock*> bb_to_clone_list = list_of_bb_to_clone_lists[i];

            for(int j=0; j<tail_list.size(); j++){
                BasicBlock* cloned_bb = tail_list[j];
                BasicBlock* bb_to_clone = bb_to_clone_list[j];
                errs() << "BB: " << *cloned_bb << "\n";

                for(Instruction& bb_inst : *bb_to_clone){
                    if(!bb_inst.getType()->isVoidTy()){
                        //if the instruction returns something, then must find the matching instruction in cloned_bb and fix any following uses
                        for(Instruction& clone_inst : *cloned_bb){
                            if(clone_inst.isIdenticalTo(&bb_inst)){ 
                                errs() << clone_inst << " is identical to " << bb_inst << "\n";
                                //then need to go through each block in duplicated tail and replaces uses of bb_inst with clone_inst
                                Value* bb_inst_val = dyn_cast<Value>(&bb_inst);
                                Value* clone_inst_val = dyn_cast<Value>(&clone_inst);

                                
                                for(auto user : bb_inst_val->users()){  // get all users (instructions) of the value
                                    Instruction* temp_i = dyn_cast<Instruction>(user);
                                    BasicBlock* temp_bb = temp_i->getParent();
                                    errs() << "The user of the bb_inst_val is " << *temp_i << "\n";
                                    //if the basicblock that uses the instruction is in the tail list, replace with the cloned inst value
                                    if(std::find(tail_list.begin(), tail_list.end(), temp_bb) != tail_list.end()){
                                        //I can't actually change the instruction here, because then future instructions are 
                                        //no longer identical when they should be. Change only after this loop finishes. 
                                        errs() << "replacing " << *clone_inst_val << " with " << *bb_inst_val << "\n";
                                        user_list.push_back(user);
                                        bb_inst_val_list.push_back(bb_inst_val);
                                        clone_inst_val_list.push_back(clone_inst_val);
                                        //user->replaceUsesOfWith(bb_inst_val, clone_inst_val);
                                    }

                                }
                            }
                        }
                    }
                }
            }
        }
        //replace register uses in tail duplicated BBs with their cloned value counterparts
        for(int i=0; i<user_list.size(); i++){
            user_list[i]->replaceUsesOfWith(bb_inst_val_list[i], clone_inst_val_list[i]);
        }

        //print out basic blocks
        // for (BasicBlock &BB : F){
        //     errs() << "Basic Block: " << BB << "\n";
        // }

        //calculate accuracy compared to profile information
        double acc = getAccuracy(F, bpi, li);
        errs() << "Accuracy is: " << acc << "\n";

    
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