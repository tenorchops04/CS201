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
        bool isFromBin = false;
        bool already_UE_l = false;
        bool already_UE_r = false;
        bool first_load = true;
        map<string, set<llvm::Value*>> m_UE;
        map<string, set<llvm::Value*>> m_VK;

        // STEP 1: Compute the upward exposed variables and killed variables for each basic block. Traverses
        // the CFG in a top-down manner
        for(auto& basic_block: F){
            string block = formatv("{0}", &basic_block);

            set<llvm::Value*> UEVars, VarKilled;

            for(auto& inst: basic_block){

// --------------- STORE instruction
                if(inst.getOpcode() == Instruction::Store){
                    llvm::Value* l = inst.getOperand(0);
                    llvm::Value* r = inst.getOperand(1);

                    // Need to check if the operand we are storing is coming from a binary operation. The variable
                    // is upwards exposed only if it is not from a binary operation
                    if(isFromBin)
                        isFromBin = false;
                    else
                        UEVars.insert(l);

                    // STORE instructions act like assignments, so it's right operand is always killed
                    VarKilled.insert(r);
                }

//---------------- LOAD instruction
                if(inst.getOpcode() == Instruction::Load){
                    llvm::Value* op0 = inst.getOperand(0);

                    // We only care about the left operand in a load instruction. If it has not been killed, add it
                    // to the upwards exposed variable set
                    if (VarKilled.find(op0) == VarKilled.end())
                        UEVars.insert(op0);
                }

//---------------- Binary operations (+, -, *, /)
                if(inst.isBinaryOp()){
                    auto op0 = dyn_cast<User>(inst.getOperand(0));
                    auto op1 = dyn_cast<User>(inst.getOperand(1));

                    llvm::Value* l = inst.getOperand(0);
                    llvm::Value* r = inst.getOperand(1);

                    // Must check if either of the operands are constants
                    if(ConstantInt* CI0 = dyn_cast<ConstantInt>(op0)) {
                    }
                    else
                        l = op0->getOperand(0);

                    if(ConstantInt* CI1 = dyn_cast<ConstantInt>(op1)) {
                    }
                    else
                        r = op1->getOperand(0);

                    // Determine the variables the upwards exposed variables
                    if(VarKilled.find(l) == VarKilled.end())
                        UEVars.insert(l);

                    if(VarKilled.find(r) == VarKilled.end())
                        UEVars.insert(r);

                    isFromBin = true;
                }

                m_UE[block] = UEVars;
                m_VK[block] = VarKilled;
            }
        }

        // STEP 2: Iterative algorithm. Calculates the LiveOut set of variables by traversing the CFG in a bottom
        // up traversal
        bool cont = true;
        map<string, set<llvm::Value*>> m_LO;

        while(cont){
            cont = false;

            for(auto& basic_block: reverse(F)){
                string block = formatv("{0}", &basic_block);
                set<llvm::Value*> prev_LO;

                copy(m_LO[block].begin(),m_LO[block].end(),std::inserter(prev_LO, prev_LO.end()));

                // Iterate through each successor X of block N
                for (BasicBlock *Succ : successors(&basic_block)) {
                    set<llvm::Value*> temp;
                    string succ = formatv("{0}", Succ);
                    set<llvm::Value*> s1 =m_LO[succ];
                    set<llvm::Value*> s2 =m_VK[succ];

                    // LiveOut(X) - VarKilled(X) U UEVar(X)
                    std::set_difference(s1.begin(), s1.end(), s2.begin(), s2.end(), std::inserter(temp, temp.end()));
                    std::set_union(temp.begin(), temp.end(), m_UE[succ].begin(), m_UE[succ].end(), std::inserter(temp, temp.end()));

                    // Computes LiveOut(N)
                    std::set_union(temp.begin(), temp.end(), m_LO[block].begin(), m_LO[block].end(), std::inserter(m_LO[block], m_LO[block].end()));
                }

                if (m_LO[block] != prev_LO) {
                    cont = true;
                }

            }
        }

        // Output UE vars, killed vars, and live out set per block
        for(auto& basic_block: F){
            string block_name = formatv("{0}",basic_block.getName());
            errs() << "-------" << block_name << "--------\n";
            string block = formatv("{0}", &basic_block);
            set<llvm::Value*>::iterator itr;
            errs() << "UEVARS: ";
            for (auto a:m_UE[block]) {
                if(a->hasName())
                    errs() << formatv("{0}",a->getName()) << " ";
            }
            errs() << "\n";
            errs() << "VARKILL: ";
            for (auto a:m_VK[block]) {
                if(a->hasName())
                    errs() << formatv("{0}",a->getName()) << " ";
            }
            errs() << "\n";
            errs() << "LIVEOUT: ";
            for (auto a:m_LO[block]) {
                if(a->hasName())
                    errs() << formatv("{0}",a->getName()) << " ";
            }
            errs() << "\n";

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
