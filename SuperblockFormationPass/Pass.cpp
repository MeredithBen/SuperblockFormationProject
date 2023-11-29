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

#include <iostream>

using namespace llvm;

namespace {

struct SuperblockFormationPass : public PassInfoMixin<SuperblockFormationPass> {

    PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {
        llvm::BlockFrequencyAnalysis::Result &bfi = FAM.getResult<BlockFrequencyAnalysis>(F);
        llvm::BranchProbabilityAnalysis::Result &bpi = FAM.getResult<BranchProbabilityAnalysis>(F);
        llvm::LoopAnalysis::Result &li = FAM.getResult<LoopAnalysis>(F);
        //llvm::LoopNestAnalysis::Result &lni = FAM.getResult<LoopNestAnalysis>(F);
        

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
                // also add the loop along with the depth to a list to be used later
            for (Loop *SL : current_loop->getSubLoops()) {
                errs() << "subloop: " << *SL << "\n";
                bfs_loops.push_back(SL);
                least_to_most_nested.push_back(SL);
            }
        }

        //sanity check: print out loop depth to check they were ordered correctly. 
        for (Loop* temp_loop : least_to_most_nested){
            errs() << "Loop depth: " << temp_loop->getLoopDepth() << "\n";
        }

        //to iterate through the list in most-nested order, access the back element of list and then pop it off.


        // This only gets each top level loop. I then need to use LoopNest class to get each nested loop. 
        //     Try to use LoopNest class rather than LoopNestAnalysis pass bc the latter requires LoopAnalysisManager
        //     Nvm! The loop depth info is somehow contained in the loop object, just need to access it

        // BasicBlock *header = L->getHeader();
        // BasicBlock *latch = L->getLoopLatch();
        // BasicBlock *preheader = L->getLoopPreheader();

        // //std::unique_ptr<LoopNest>tempNest = &L->getLoopNest();
        // errs() << "Loop nest object: " << *L << "\n";
        // errs() << "Loop nest header: " << *header << "\n";
        
        // for (Loop* loop_obj : *L){
        //     errs() << "Loop object: " << *loop_obj << "\n";
        //     errs() << "Loop header: " << *loop_obj->getHeader() << "\n";
        // }
        
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