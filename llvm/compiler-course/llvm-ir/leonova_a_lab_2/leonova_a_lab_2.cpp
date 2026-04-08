#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {

struct StrengthReductionPass : public PassInfoMixin<StrengthReductionPass> {

  PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {
    bool changed = false;

    for (auto &BB : F) {
      for (auto &I : make_early_inc_range(BB)) {

        auto *binOp = dyn_cast<BinaryOperator>(&I);
        if (!binOp)
          continue;

        Value *lhs = binOp->getOperand(0);
        Value *rhs = binOp->getOperand(1);

        ConstantInt *constOp = nullptr;
        Value *varOp = nullptr;

        // Определяем константу и переменную
        if ((constOp = dyn_cast<ConstantInt>(rhs))) {
          varOp = lhs;
        } else if ((constOp = dyn_cast<ConstantInt>(lhs))) {
          varOp = rhs;
        } else {
          continue;
        }

        // Проверяем, что константа - степень 2 (для mul/div)
        if (!constOp->getValue().isPowerOf2())
          continue;

        IRBuilder<> builder(binOp);
        Value *newInstr = nullptr;

        switch (binOp->getOpcode()) {

        case Instruction::Mul: {
          unsigned shiftAmount = constOp->getValue().abs().logBase2();
          Value *shifted = builder.CreateShl(varOp, shiftAmount);

          // Если константа отрицательная, умножаем результат на -1
          if (constOp->getValue().isNegative()) {
            Value *negOne = ConstantInt::get(varOp->getType(), -1, true);
            shifted = builder.CreateMul(shifted, negOne);
          }
          newInstr = shifted;
          break;
        }

        case Instruction::UDiv: {
          // Для unsigned деления работает только для положительных констант
          if (constOp->getValue().isStrictlyPositive()) {
            unsigned shiftAmount = constOp->getValue().logBase2();
            newInstr = builder.CreateLShr(varOp, shiftAmount);
          }
          break;
        }

        case Instruction::SDiv: {
          unsigned shiftAmount = constOp->getValue().abs().logBase2();
          Type *Ty = varOp->getType();

          // bias = (1 << k) - 1
          APInt biasVal =
              APInt::getLowBitsSet(Ty->getIntegerBitWidth(), shiftAmount);
          Value *bias = ConstantInt::get(Ty, biasVal);

          // Если x < 0, добавляем bias, иначе 0
          Value *isNeg = builder.CreateICmpSLT(varOp, ConstantInt::get(Ty, 0));
          Value *adjusted = builder.CreateAdd(
              varOp,
              builder.CreateSelect(isNeg, bias, ConstantInt::get(Ty, 0)));

          // AShr для деления на положительную степень 2
          Value *divPos = builder.CreateAShr(adjusted, shiftAmount);

          // Если константа отрицательная, умножаем результат на -1
          if (constOp->getValue().isNegative()) {
            Value *negOne = ConstantInt::get(Ty, -1, true);
            divPos = builder.CreateMul(divPos, negOne);
          }

          newInstr = divPos;
          break;
        }

        default:
          continue;
        }

        if (newInstr) {
          binOp->replaceAllUsesWith(newInstr);
          binOp->eraseFromParent();
          changed = true;
        }
      }
    }

    return changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
  }

  static bool isRequired() { return true; }
};

} // namespace

extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "PowOf2ToShift", "0.1", [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (name == "pow-of-2-to-shift") {
                    FPM.addPass(StrengthReductionPass());
                    return true;
                  }
                  return false;
                });
          }};
}