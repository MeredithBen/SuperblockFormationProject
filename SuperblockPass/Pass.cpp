#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

#include <iostream>

using namespace llvm;

namespace {

struct SuperblockPass : public PassInfoMixin<SuperblockPass> {

    PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {

        std::cout << "Hello function: " << F.getName().str() << std::endl;
        return PreservedAnalyses::all();
    }
};
}

extern "C" ::llvm::PassPluginLibraryInfo LLVM_ATTRIBUTE_WEAK llvmGetPassPluginInfo() {
    return {
        LLVM_PLUGIN_API_VERSION, "SuperblockPass", "v0.1",
        [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                ArrayRef<PassBuilder::PipelineElement>) {
                    if(Name == "func-name"){
                        FPM.addPass(SuperblockPass());
                        return true;
                    }
                    return false;
                }
            );
        }
    };
}