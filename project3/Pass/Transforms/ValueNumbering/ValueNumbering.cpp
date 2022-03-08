#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/FormatVariadic.h"
#include<map>
#include<set>

using namespace std;
using namespace llvm;

namespace {
    void LivenessAnalysisPass(Function &F){
        set<string> UEVars, VarKilled;
        for(auto& basic_block: F){
            for(auto& inst: basic_block){
// --------------- STORE instruction
                if(inst.getOpcode() == Instruction::Store){
                    std::string l;
                    std::string r;

                    // Need to check if the left operand is a constant number
                    if(ConstantInt* CI = dyn_cast<ConstantInt>(inst.getOperand(0)))
                        l = formatv("{0}", CI->getSExtValue());
                    else
                        l = formatv("{0}", inst.getOperand(0));

                    // The right operand will always be the address were the left operand is stored
                    r = formatv("{0}", inst.getOperand(1));
                }
//---------------- Binary operations (+, -, *, /)
                    if(inst.isBinaryOp()){
                        auto op0 = dyn_cast<User>(inst.getOperand(0));
                        auto op1 = dyn_cast<User>(inst.getOperand(1));

                        std::string l;
                        std::string r;

                        // Must check if either of the operands are constants
                        if(ConstantInt* CI0 = dyn_cast<ConstantInt>(op0))
                            l = formatv("{0}", CI0->getSExtValue());
                        else
                            l = formatv("{0}", op0->getOperand(0));

                        if(ConstantInt* CI1 = dyn_cast<ConstantInt>(op1))
                            r = formatv("{0}", CI1->getSExtValue());
                        else
                            r = formatv("{0}", op1->getOperand(0));

                        if(VarKilled.find(l) != VarKilled.end()){
                            UEVars.insert(l);
                        }
                        if(VarKilled.find(r) != VarKilled.end()){
                            UEVars.insert(r);
                        }

                        // Need var for assignment var
                }
            }
        }
    }

    // New PM implementation
    struct ValueNumberingPass : public PassInfoMixin<ValueNumberingPass> {

          // The first argument of the run() function defines on what level
          // of granularity your pass will run (e.g. Module, Function).
          // The second argument is the corresponding AnalysisManager
          // (e.g ModuleAnalysisManager, FunctionAnalysisManager)
        PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {
            // visitor(F);
            LivenessAnalysisPass(F);
            return PreservedAnalyses::all();
        }

        static bool isRequired() { return true; }
    };
}



//-----------------------------------------------------------------------------
// New PM Registration
//-----------------------------------------------------------------------------
extern "C" ::llvm::PassPluginLibraryInfo LLVM_ATTRIBUTE_WEAK
llvmGetPassPluginInfo() {
  return {
    LLVM_PLUGIN_API_VERSION, "ValueNumberingPass", LLVM_VERSION_STRING,
    [](PassBuilder &PB) {
      PB.registerPipelineParsingCallback(
        [](StringRef Name, FunctionPassManager &FPM,
        ArrayRef<PassBuilder::PipelineElement>) {
          if(Name == "value-numbering"){
            FPM.addPass(ValueNumberingPass());
            return true;
          }
          return false;
        });
    }};
}
