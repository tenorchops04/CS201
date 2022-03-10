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
        map<string, set<llvm::Value*>> m_UE_val;
        map<string, set<llvm::Value*>> m_VK_val;

        // Computes the upward exposed variables and killed variables for each basic block
        for(auto& basic_block: F){
            string block = formatv("{0}", &basic_block);
            errs() << "Block: " << block << '\n';
            set<string> UEVars, VarKilled;
            set<llvm::Value*> UEVars_val, VarKilled_val;

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
                    VarKilled_val.insert(inst.getOperand(1));
                    errs() << "\tVar killed: " << r << '\n';
                }
//---------------- Binary operations (+, -, *, /)
                if(inst.isBinaryOp()){
                    errs() << "\tBinary op\n";
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
                    if(VarKilled.find(l) == VarKilled.end()){
                        UEVars.insert(l);
                        UEVars_val.insert(l1);
                        errs() << "\tUEVars: " << l << '\n';
                    }
                    if(VarKilled.find(r) == VarKilled.end()){
                        UEVars.insert(r);
                        UEVars_val.insert(r1);
                        errs() << "\tUEVars: " << r << '\n';
                    }
                }
                m_UE[block] = UEVars;
                m_VK[block] = VarKilled;
                m_UE_val[block] = UEVars_val;
                m_VK_val[block] = VarKilled_val;

            }
        }

        bool cont = true;
        map<string, set<string>> m_LO;
        map<string, set<llvm::Value*>> m_LO_val;
        errs() << "Bottom up\n\n";

        while(cont){
            errs() << "iteration\n";
            cont = false;

            for(auto& basic_block: reverse(F)){
                string block = formatv("{0}", &basic_block);
                vector<string> prev_LO;
                vector<llvm::Value*> prev_LO_val;
                errs() << "\tblock: " << block << "\n";

                map<string, set<string>>::iterator f = m_LO.find(block);
                map<string, set<llvm::Value*>>::iterator f1 = m_LO_val.find(block);

                if(f != m_LO.end()){
                    errs() << "Found block in m_LO\n";
                    copy(m_LO[block].begin(),m_LO[block].end(),std::back_inserter(prev_LO));
                }
                if(f1 != m_LO_val.end()){
                    errs() << "Found block in m_LO\n";
                    copy(m_LO_val[block].begin(),m_LO_val[block].end(),std::back_inserter(prev_LO_val));
                }

                errs() << "\tm_LO[block]\n";
                vector<string> new_lo;
                vector<llvm::Value*> new_lo_val;

                for (BasicBlock *Succ : successors(&basic_block)) {
                    vector<string> temp;
                    vector<llvm::Value*> temp2;
                    string succ = formatv("{0}", Succ);
                    errs() << "\tsuccessor: " << succ << '\n';
                    set<string> s1 =m_LO[succ];
                    set<string> s2 =m_VK[succ];
                    set<llvm::Value*> s3 =m_LO_val[succ];
                    set<llvm::Value*> s4 =m_VK_val[succ];

                    std::set_difference(s1.begin(), s1.end(), s2.begin(), s2.end(), std::back_inserter(temp));
                    std::set_union(temp.begin(), temp.end(), m_UE[succ].begin(), m_UE[succ].end(), std::back_inserter(temp));

                    std::set_union(temp.begin(), temp.end(), new_lo.begin(), new_lo.end(), std::back_inserter(new_lo));

                    std::set_difference(s3.begin(), s3.end(), s4.begin(), s4.end(), std::back_inserter(temp2));
                    std::set_union(temp2.begin(), temp2.end(), m_UE_val[succ].begin(), m_UE_val[succ].end(), std::back_inserter(temp2));

                    std::set_union(temp2.begin(), temp2.end(), new_lo_val.begin(), new_lo_val.end(), std::back_inserter(new_lo_val));
                }

                set<string> new_LO_set;
                copy(new_lo.begin(),new_lo.end(),inserter(new_LO_set,new_LO_set.end()));

                set<string> prev_LO_set;
                copy(prev_LO.begin(),prev_LO.end(),inserter(prev_LO_set,prev_LO_set.end()));
                set<llvm::Value*> new_LO_set_val;
                copy(new_lo_val.begin(),new_lo_val.end(),inserter(new_LO_set_val,new_LO_set_val.end()));

                set<llvm::Value*> prev_LO_set_val;
                copy(prev_LO_val.begin(),prev_LO_val.end(),inserter(prev_LO_set_val,prev_LO_set_val.end()));
                errs() << "new_lo: ";
                for(auto a: new_LO_set)
                    errs() << a << " ";
                errs() << '\n';

                errs() << "prev_LO: ";
                for(auto a: prev_LO_set)
                    errs() << a << " ";
                errs() << '\n';

                errs() << "Checking changed\n";

                if (new_LO_set != prev_LO_set) { 
                    errs() << "Inside if stmt\n";
                    //set<string> new_LO_set;
                    //copy(new_lo.begin(),new_lo.end(),inserter(new_LO_set,new_LO_set.end()));
                    m_LO[block] = new_LO_set;
                    m_LO_val[block] = new_LO_set_val;
                    cont = true;
                    errs() << "continue\n";
                }
                errs() << "After checking\n";

            }
        }
        for(auto& basic_block: F){
            string block_name = formatv("{0}",basic_block.getName());
            errs() << "-------" << block_name << "--------\n";
            string block = formatv("{0}", &basic_block);
            //map<string,set<string>>::iterator f = m_LO.find(block);
            set<llvm::Value*>::iterator itr;
            errs() << "UEVARS: ";
            for (auto a:m_UE_val[block]) {
                errs() << formatv("{0}",a->getName()) << " ";
            }
            errs() << "\n";
            errs() << "VARKILL: ";
            for (auto a:m_VK_val[block]) {
                errs() << formatv("{0}",a->getName()) << " ";
            }
            errs() << "\n";
            errs() << "LIVEOUT: ";
//            for (auto a:m_LO_val[block]) {
//                errs() << formatv("{0}",a->getName()) << " ";
//            }
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
