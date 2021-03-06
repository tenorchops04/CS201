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
    void LVN(Function &F){
        errs() << "ValueNumbering: " << F.getName() << '\n';

        int ID = 1;
        std::map<std::string, int> hash;
        std::map<std::string, int>::iterator it, it1;

        for(auto& basic_block : F){
            for(auto& inst : basic_block){
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

                    // Search for the value numbers of the left and right operands
                    it = hash.find(l);
                    it1 = hash.find(r);

                    // If the left operand is not found, insert both operands into the hash table with the same ID
                    if(it == hash.end()){
                        hash[l] = ID;
                        hash[r] = ID;
                        errs() << formatv("{0, -40} {1} = {2}\n", inst, hash[l], hash[r]);
                        ID++;
                    }

                    // If the left operand is found, then map the right value to the left value
                    else if (it != hash.end()){
                        hash[r] = hash[l];
                        errs() << formatv("{0, -40} {1} = {2}\n", inst, hash[l], hash[r]);
                    }
                }

//---------------- LOAD instruction
                if(inst.getOpcode() == Instruction::Load){
                    std::string instr = formatv("{0}", &inst);
                    std::string op = formatv("{0}", inst.getOperand(0));

                    it = hash.find(instr);
                    it1 = hash.find(op);

                    if(it1 != hash.end()){
                        hash[instr] = hash[op];
                        errs() << formatv("{0,-40} {1} = {2}\n", inst, hash[instr], hash[op]); 
                    }
                    else{
                        errs() << "Could not find operand(0)\n";
                        errs() << formatv("{0,-40} {1} = {2}\n", inst, hash[instr], hash[instr]); 
                    }
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

                    // Search for the value numbers of the left and right operands
                    it =  hash.find(l);
                    it1 = hash.find(r);

                    // If not found, create new entries in the hash table
                    if(it == hash.end()){
                        hash[l] = ID;
                        ID++;
                    }

                    if(it1 == hash.end()){
                        hash[r] = ID;
                        ID++;
                    }

                    auto opcode = inst.getOpcode();
                    std::string s, instr, op_str;
                    instr = formatv("{0}", &inst);

                    switch(opcode){
                        case Instruction::Add:
                            op_str = "add";
                            break;
                        case Instruction::Sub:
                            op_str = "sub";
                            break;
                        case Instruction::Mul:
                            op_str = "mul";
                            break;
                        default:
                            break;
                    }
                    s = llvm::formatv("{0} {1} {2}", hash[l], op_str, hash[r]);

                    // Search the op hash table for V(l) op V(r)
                    it = hash.find(s);

                    // It not found, create a new entry in the hash table
                    if(it == hash.end()){
                        hash[s] = ID;
                        hash[instr] = ID;
                        errs() << formatv("{0, -40} {1} = {2}\n", inst, hash[s], s);
                        ID++;

                    }
                    else{
                        hash[instr] = hash[s];
                        errs() << formatv("{0, -40} {1} {4} {2} {3}\n", inst, hash[s], s, "(redundant)", '=');
                    }
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
