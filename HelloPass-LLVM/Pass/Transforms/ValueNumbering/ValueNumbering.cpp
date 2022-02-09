#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/FormatVariadic.h"
#include<map>
using namespace std;

using namespace llvm;

namespace {
    // This method implements what the pass does
    void visitor(Function &F){

        // Here goes what you want to do with a pass

        string func_name = "main";
        errs() << "ValueNumbering: " << F.getName() << "\n";

        // Comment this line
        // if (F.getName() != func_name) return;

        for (auto& basic_block : F)
        {
            for (auto& inst : basic_block)
            {
                errs() << inst << "\n";
                if(inst.getOpcode() == Instruction::Load){
                    errs() << "This is Load"<<"\n";
                }
                if(inst.getOpcode() == Instruction::Store){
                    errs() << "This is Store"<<"\n";
                }
                if (inst.isBinaryOp())
                {
                    errs() << "Op Code:" << inst.getOpcodeName()<<"\n";
                    if(inst.getOpcode() == Instruction::Add){
                        errs() << "This is Addition"<<"\n";
                    }
                    if(inst.getOpcode() == Instruction::Load){
                        errs() << "This is Load"<<"\n";
                    }
                    if(inst.getOpcode() == Instruction::Mul){
                        errs() << "This is Multiplication"<<"\n";
                    }

                    // see other classes, Instruction::Sub, Instruction::UDiv, Instruction::SDiv
                    // errs() << "Operand(0)" << (*inst.getOperand(0))<<"\n";
                    auto* ptr = dyn_cast<User>(&inst);
                    //errs() << "\t" << *ptr << "\n";
                    for (auto it = ptr->op_begin(); it != ptr->op_end(); ++it) {
                        errs() << "\t" <<  *(*it) << "\n";
                        // if ((*it)->hasName()) 
                        // errs() << (*it)->getName() << "\n";                      
                    }
                } // end if
            } // end for inst
        } // end for block
    }

    void LVN(Function &F){
        errs() << "Function name: " << F.getName() << '\n';

        int ID = 1;
        std::map<llvm::Value*, int> hash;
        std::map<llvm::Value*, int>::iterator it, it1;
        std::map<std::string, int> op_hash;
        std::map<std::string, int>::iterator op_it;

        for(auto& basic_block : F){

            for(auto& inst : basic_block){
                if(inst.getOpcode() == Instruction::Store){
                    errs() << "Store: " << '\n';
                    errs() << '\t' << inst;

                    // Search for the value numbers of the left and right operands
                    it = hash.find(inst.getOperand(0));
                    it1 = hash.find(inst.getOperand(1));

                    // If the left operand is not found, insert both operands into the hash table with the same ID
                    if(it == hash.end()){
                        hash[inst.getOperand(0)] = ID;
                        hash[inst.getOperand(1)] = ID;
                        errs() << "\t" << hash[inst.getOperand(0)] << " = " << hash[inst.getOperand(1)] << '\n'; 
                        ID++;
                    }
                    // If the left operand is found, then map the right value to the left value
                    else{
                        hash[inst.getOperand(1)] = hash[inst.getOperand(0)];
                        errs() << "\t" << hash[inst.getOperand(0)] << " = " << hash[inst.getOperand(1)] << '\n'; 
                    }
                    errs() << "\tOperand(0):" << *(inst.getOperand(0)) << '\n';
                    errs() << "\tOperand(1):" << *(inst.getOperand(1)) << '\n';
                }

                if(inst.getOpcode() == Instruction::Load){
                    errs() << "Load: " << '\n';
                    errs() << '\t' << inst;

                    it = hash.find(inst.getOperand(0));

                    if(it == hash.end()){
                        hash[inst.getOperand(0)] = ID;
                        errs() << "\t" << hash[inst.getOperand(0)] << " = " << hash[inst.getOperand(0)] << '\n'; 
                        ID++;
                    }
                    else{
                        errs() << "\t" << hash[inst.getOperand(0)] << " = " << hash[inst.getOperand(0)] << '\n'; 
                    }
                    errs() << "\tOperand(0):" << *(inst.getOperand(0)) << '\n';
                }

                if(inst.isBinaryOp()){
                    if(inst.getOpcode() == Instruction::Add){
                        errs() << "Add: " << '\n';
                        errs() << '\t' << inst;

                        auto l = dyn_cast<User>(inst.getOperand(0));
                        auto r = dyn_cast<User>(inst.getOperand(1));

                        // Search for the value numbers of the left and right operands
                        it =  hash.find(l->getOperand(0));
                        it1 = hash.find(r->getOperand(0));

                        // If not found, create new entries in the hash table
                        if(it == hash.end()){
                            hash[l->getOperand(0)] = ID;
                            ID++;
                        }

                        if(it1 == hash.end()){
                            hash[r->getOperand(0)] = ID;
                            ID++;
                        }

                        std::string s;
                        s = llvm::formatv("{0} {1} {2}", hash[l->getOperand(0)], '+', hash[r->getOperand(0)]);

                        // Search the op hash table for V(l) op V(r)
                        op_it = op_hash.find(s);

                        // It not found, create a new entry in the hash table
                        if(op_it == op_hash.end()){
                            hash[&inst] = ID;
                            op_hash[s] = ID;
                            errs() << "\t\t" <<  op_hash[s] << " = " << hash[l->getOperand(0)] << " + " << hash[r->getOperand(0)] << '\n';
                            ID++;

                        }
                        else{
                            errs() << "\t\t" <<  op_hash[s] << " = " << hash[l->getOperand(0)] << " + " << hash[r->getOperand(0)] << '\n';
                        }

                        errs() << "\tOperand(0):" << *(l->getOperand(0)) << '\n';
                        errs() << "\tOperand(1):" << *(r->getOperand(0)) << '\n';
                    }

                    if(inst.getOpcode() == Instruction::Sub)
                        errs() << "Sub: " << inst.getOperand(0) << " " << inst.getOperand(1) << '\n';
                    if(inst.getOpcode() == Instruction::Mul)
                        errs() << "Mult: " << inst.getOperand(0) << " " << inst.getOperand(1) << '\n';
                    if(inst.getOpcode() == Instruction::UDiv)
                        errs() << "UDiv: " << inst.getOperand(0) << " " << inst.getOperand(1) << '\n';
                    if(inst.getOpcode() == Instruction::SDiv)
                        errs() << "SDiv: " << inst.getOperand(0) << " " << inst.getOperand(1) << '\n';

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
            LVN(F);
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
