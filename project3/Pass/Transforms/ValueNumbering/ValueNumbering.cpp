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
        map<string, set<string>> m_UE;
        map<string, set<string>> m_VK;
        map<string, set<llvm::Value*>> m_UE_val;
        map<string, set<llvm::Value*>> m_VK_val;

        // STEP 1: Compute the upward exposed variables and killed variables for each basic block. Traverses
        // the CFG in a top-down manner
        for(auto& basic_block: F){
            string block = formatv("{0}", &basic_block);

            set<string> UEVars, VarKilled;
            set<llvm::Value*> UEVars_val, VarKilled_val;

            for(auto& inst: basic_block){
                string instr = formatv("{0}", &inst);

// --------------- STORE instruction
                if(inst.getOpcode() == Instruction::Store){
                    std::string l;
                    std::string r;
                    llvm::Value* l1 = inst.getOperand(0);
                    llvm::Value* r1 = inst.getOperand(1);

                    // Need to check if the left operand is a constant number
                    if(ConstantInt* CI = dyn_cast<ConstantInt>(inst.getOperand(0)))
                        l = formatv("{0}", CI->getSExtValue());
                    else
                        l = formatv("{0}", inst.getOperand(0));

                    // The right operand will always be the address were the left operand is stored
                    r = formatv("{0}", inst.getOperand(1));

                    // Need to check if the operand we are storing is coming from a binary operation. The variable
                    // is upwards exposed only if it is not from a binary operation
                    if(isFromBin)
                        isFromBin = false;
                    else{
                        UEVars.insert(l);
                        UEVars_val.insert(l1);

                    }

                    // STORE instructions act like assignments, so it's right operand is always killed
                    VarKilled.insert(r);
                    VarKilled_val.insert(r1);
                }

//---------------- LOAD instruction
                if(inst.getOpcode() == Instruction::Load){
                    std::string op = formatv("{0}", inst.getOperand(0));
                    llvm::Value* op0 = inst.getOperand(0);

                    // We only care about the left operand in a load instruction. If it has not been killed, add it
                    // to the upwards exposed variable set
                    if (VarKilled_val.find(op0) == VarKilled_val.end()) {
                        UEVars.insert(op);
                        UEVars_val.insert(op0);
                    }
                }

//---------------- Binary operations (+, -, *, /)
                if(inst.isBinaryOp()){
                    auto op0 = dyn_cast<User>(inst.getOperand(0));
                    auto op1 = dyn_cast<User>(inst.getOperand(1));

                    std::string l;
                    llvm::Value* l1 = inst.getOperand(0);
                    std::string r;
                    llvm::Value* r1 = inst.getOperand(1);

                    // Must check if either of the operands are constants
                    if(ConstantInt* CI0 = dyn_cast<ConstantInt>(op0)) {
                        l = formatv("{0}", CI0->getSExtValue());
                    }
                    else {
                        l = formatv("{0}", op0->getOperand(0));
                        l1 = op0->getOperand(0);
                    }

                    if(ConstantInt* CI1 = dyn_cast<ConstantInt>(op1)) {
                        r = formatv("{0}", CI1->getSExtValue());
                    }
                    else {
                        r = formatv("{0}", op1->getOperand(0));
                        r1 = op1->getOperand(0);
                    }

                    // Determine the variables the upwards exposed variables
                    if(VarKilled_val.find(l1) == VarKilled_val.end()){
                        UEVars.insert(l);
                        UEVars_val.insert(l1);
                    }
                    if(VarKilled_val.find(r1) == VarKilled_val.end()){
                        UEVars.insert(r);
                        UEVars_val.insert(r1);
                    }
                    isFromBin = true;


                }

                m_UE[block] = UEVars;
                m_VK[block] = VarKilled;
                m_UE_val[block] = UEVars_val;
                m_VK_val[block] = VarKilled_val;
            }

            already_UE_l = false;
            already_UE_r = false;
        }

        // STEP 2: Iterative algorithm. Calculates the LiveOut set of variables by traversing the CFG in a bottom
        // up traversal
        bool cont = true;
        map<string, set<string>> m_LO;
        map<string, set<llvm::Value*>> m_LO_val;

        while(cont){
            cont = false;

            for(auto& basic_block: reverse(F)){
                string block = formatv("{0}", &basic_block);
                set<string> prev_LO;
                set<llvm::Value*> prev_LO_val;

                map<string, set<string>>::iterator f = m_LO.find(block);
                map<string, set<llvm::Value*>>::iterator f1 = m_LO_val.find(block);

                if(f != m_LO.end()){
                    copy(m_LO[block].begin(),m_LO[block].end(),std::inserter(prev_LO, prev_LO.end()));
                }
                if(f1 != m_LO_val.end()){
                    copy(m_LO_val[block].begin(),m_LO_val[block].end(),std::inserter(prev_LO_val, prev_LO_val.end()));
                }

                set<string> new_lo;
                set<llvm::Value*> new_lo_val;

                for (BasicBlock *Succ : successors(&basic_block)) {
                    set<string> temp;
                    set<llvm::Value*> temp2;
                    string succ = formatv("{0}", Succ);
                    set<string> s1 =m_LO[succ];
                    set<string> s2 =m_VK[succ];
                    set<llvm::Value*> s3 =m_LO_val[succ];
                    set<llvm::Value*> s4 =m_VK_val[succ];

                    std::set_difference(s1.begin(), s1.end(), s2.begin(), s2.end(), std::inserter(temp, temp.end()));
                    std::set_union(temp.begin(), temp.end(), m_UE[succ].begin(), m_UE[succ].end(), std::inserter(temp, temp.end()));

                    std::set_union(temp.begin(), temp.end(), new_lo.begin(), new_lo.end(), std::inserter(new_lo, new_lo.end()));

                    std::set_difference(s3.begin(), s3.end(), s4.begin(), s4.end(), std::inserter(temp2, temp2.end()));
                    std::set_union(temp2.begin(), temp2.end(), m_UE_val[succ].begin(), m_UE_val[succ].end(), std::inserter(temp2, temp2.end()));

                    std::set_union(temp2.begin(), temp2.end(), new_lo_val.begin(), new_lo_val.end(), std::inserter(new_lo_val, new_lo_val.end()));
                }

                if (new_lo_val != prev_LO_val) { 
                    m_LO[block] = new_lo;
                    m_LO_val[block] = new_lo_val;
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
            for (auto a:m_UE_val[block]) {
                if(a->hasName())
                    errs() << formatv("{0}",a->getName()) << " ";
            }
            errs() << "\n";
            errs() << "VARKILL: ";
            for (auto a:m_VK_val[block]) {
                if(a->hasName())
                    errs() << formatv("{0}",a->getName()) << " ";
            }
            errs() << "\n";
            errs() << "LIVEOUT: ";
            for (auto a:m_LO_val[block]) {
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
