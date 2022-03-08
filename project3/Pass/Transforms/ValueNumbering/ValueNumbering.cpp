#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/IR/BasicBlock.h"
#include<map>
#include<set>

using namespace std;
using namespace llvm;

namespace {
    void LivenessAnalysisPass(Function &F){
        map<string, set<string>> m_UE;
        map<string, set<string>> m_VK;
        map<string, set<string>> m_LO;

        // Computes the upward exposed variables and killed variables for each basic block
        for(auto& basic_block: F){
            string block = formatv("{0}", &basic_block);
            errs() << "Block: " << block << '\n';
            set<string> UEVars, VarKilled;

            for(auto& inst: basic_block){
                string instr = formatv("{0}", &inst);
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

                    VarKilled.insert(r);
                    errs() << "\tVar killed: " << r << '\n';
                }
//---------------- Binary operations (+, -, *, /)
                if(inst.isBinaryOp()){
                    errs() << "\tBinary op\n";
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

                    // Determine the variables the upwards exposed variables
                    if(VarKilled.find(l) == VarKilled.end()){
                        UEVars.insert(l);
                        errs() << "\tUEVars: " << l << '\n';
                    }
                    if(VarKilled.find(r) == VarKilled.end()){
                        UEVars.insert(r);
                        errs() << "\tUEVars: " << r << '\n';
                    }
                }
                m_UE[block] = UEVars;
                m_VK[block] = VarKilled;

            }
        }

        bool cont = true;

        while(cont){
            cont = false;
            for(auto& basic_block: reverse(F)){
                string block = formatv("{0}", &basic_block);
                set<string> temp;
                for (BasicBlock *Succ : successors(&basic_block)) {
                    string succ = formatv("{0}", Succ);

                    std::set_difference(m_LO[succ].begin(), m_LO[succ].end(), m_VK[succ].begin(), m_VK[succ].end(), temp);
                    std::set_union(temp.begin(), temp.end(), m_UE[succ].begin(), m_UE[succ].end(), temp);

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
