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

#include <iostream>

using namespace llvm;

namespace {

// ------------------------------------ top level heuristics function ---------------------------------------------------
BasicBlock* getLikelyBlock(BasicBlock* block){
    //implement function to return the most likely successor of a basic block
        // for now, returns just the first successor
    BasicBlock* temp;
    for(BasicBlock* bb : successors(block)){
        temp = bb;
    }
    return temp;
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
            likely_block = getLikelyBlock(current_block); //needs to account for coming out of the loop -- loop heuristic should do that.
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
        

struct SuperblockFormationPass : public PassInfoMixin<SuperblockFormationPass> {

    PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {
        // llvm::BlockFrequencyAnalysis::Result &bfi = FAM.getResult<BlockFrequencyAnalysis>(F);
        // llvm::BranchProbabilityAnalysis::Result &bpi = FAM.getResult<BranchProbabilityAnalysis>(F);
        llvm::LoopAnalysis::Result &li = FAM.getResult<LoopAnalysis>(F);
        DominatorTree dt = DominatorTree(F);

     
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
        ValueToValueMapTy VMap;
        for(Trace curr_trace : traces){
            BasicBlock* first_in_trace = curr_trace.getEntryBasicBlock();
            for(BasicBlock* curr_bb : curr_trace){
                if(curr_bb->hasNPredecessorsOrMore(2) && curr_bb != first_in_trace){
                    //if there is a block in the trace that has 2 or more predecessors, and it isn't the header, need to tail-duplicate
                    auto trace_size = curr_trace.size();
                    auto curr_index = curr_trace.getBlockIndex(curr_bb);
                    errs() << "The length of the trace is: " << trace_size << " and the index is "<< curr_index <<"\n";
                    for(int i=curr_index; i<trace_size; i++){ //for all of the blocks in the trace after the side entrance
                        BasicBlock* bb_to_clone = curr_trace.getBlock(i); 
                        BasicBlock* cloned_bb = CloneBasicBlock(bb_to_clone, VMap);
                        cloned_bb->insertInto(&F); //insert the cloned_bb into the function
                        errs() << "The cloned bb is: " << *cloned_bb <<"\n";
                        //need to change the predecessors of the bb_to_clone and the cloned_bb
                        for(BasicBlock* pred : predecessors(curr_bb)){
                            //one pred is in trace, should stay connected to bb_to_clone
                                //other pred not in trace, should renove connection to curr_bb and instead connect to cloned_bb
                            if(std::find(curr_trace.begin(), curr_trace.end(), pred) == curr_trace.end()){
                                Instruction* terminator = pred->getTerminator();
                                terminator->replaceSuccessorWith(bb_to_clone, cloned_bb);
                                errs() << "The cloned bb is now: " << *cloned_bb << "\n";
                            }
                        }
                    }
                }
            }
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