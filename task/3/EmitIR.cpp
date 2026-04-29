#include "EmitIR.hpp"
#include <llvm/Transforms/Utils/ModuleUtils.h>

#define self (*this)

using namespace asg;

EmitIR::EmitIR(Obj::Mgr& mgr, llvm::LLVMContext& ctx, llvm::StringRef mid)
  : mMgr(mgr)
  , mMod(mid, ctx)
  , mCtx(ctx)
  , mIntTy(llvm::Type::getInt32Ty(ctx))
  , mCurFunc(nullptr)
  , mCurIrb(std::make_unique<llvm::IRBuilder<>>(ctx))
  , mCtorTy(llvm::FunctionType::get(llvm::Type::getVoidTy(ctx), false))
  , mCurLoopCond(nullptr)
  , mCurLoopExit(nullptr)
{
}

llvm::Module&
EmitIR::operator()(asg::TranslationUnit* tu)
{
  for (auto&& i : tu->decls)
    self(i);
  return mMod;
}

//==============================================================================
// 类型
//==============================================================================

llvm::Type*
EmitIR::operator()(const Type* type)
{
  if (type->texp == nullptr) {
    switch (type->spec) {
      case Type::Spec::kVoid:
        return llvm::Type::getVoidTy(mCtx);
      case Type::Spec::kChar:
        return llvm::Type::getInt8Ty(mCtx);
      case Type::Spec::kInt:
        return llvm::Type::getInt32Ty(mCtx);
      case Type::Spec::kLong:
      case Type::Spec::kLongLong:
        return llvm::Type::getInt64Ty(mCtx);
      default:
        ABORT();
    }
  }

  Type subt;
  subt.spec = type->spec;
  subt.qual = type->qual;
  subt.texp = type->texp->sub;

  if (auto p = type->texp->dcst<FunctionType>()) {
    std::vector<llvm::Type*> pty;
    for (auto&& param : p->params) {
      pty.push_back(self(param));
    }
    return llvm::FunctionType::get(self(&subt), std::move(pty), false);
  }

  if (auto p = type->texp->dcst<ArrayType>()) {
    llvm::Type* elemType = self(&subt);
    return llvm::ArrayType::get(elemType, p->len);
  }

  if (auto p = type->texp->dcst<PointerType>()) {
    llvm::Type* elemType = self(&subt);
    return elemType->getPointerTo();
  }

  ABORT();
}

//==============================================================================
// 表达式
//==============================================================================

llvm::Value* EmitIR::operator()(Expr* obj) {
  if (auto p = obj->dcst<IntegerLiteral>())
    return self(p);
  
  if (auto p = obj->dcst<DeclRefExpr>())
    return self(p);
  
  if (auto p = obj->dcst<BinaryExpr>())
    return self(p);
  
  if (auto p = obj->dcst<ImplicitCastExpr>())
    return self(p);
  
  if (auto p = obj->dcst<UnaryExpr>())
    return self(p);
  
  if (auto p = obj->dcst<CallExpr>())
    return self(p);
  
  if (auto p = obj->dcst<ParenExpr>())
    return self(p);
  
  if (auto p = obj->dcst<InitListExpr>())
    return self(p);
  
  if (auto p = obj->dcst<ImplicitInitExpr>())
    return self(p);
  
  ABORT();
}

llvm::Constant*
EmitIR::operator()(IntegerLiteral* obj)
{
  return llvm::ConstantInt::get(self(obj->type), obj->val);
}

llvm::Value*
EmitIR::operator()(DeclRefExpr* obj)
{
  if (auto var = obj->decl->dcst<VarDecl>()) {
    llvm::Value* addr = static_cast<llvm::Value*>(var->any);
    if (!addr) {
      ABORT();
    }
    // 返回地址，不Load（让ImplicitCastExpr::kLValueToRValue来处理Load）
    return addr;
  }
  ABORT();
}

llvm::Value*
EmitIR::operator()(BinaryExpr* obj)
{
  llvm::Value* lhs = self(obj->lft);
  llvm::Value* rhs = self(obj->rht);
  
  switch (obj->op) {
    case BinaryExpr::kAdd:
      return mCurIrb->CreateAdd(lhs, rhs, "add");
    case BinaryExpr::kSub:
      return mCurIrb->CreateSub(lhs, rhs, "sub");
    case BinaryExpr::kMul:
      return mCurIrb->CreateMul(lhs, rhs, "mul");
    case BinaryExpr::kDiv:
      return mCurIrb->CreateSDiv(lhs, rhs, "div");
    case BinaryExpr::kMod:
      return mCurIrb->CreateSRem(lhs, rhs, "mod");
    case BinaryExpr::kAssign: {
      // lhs现在是地址（因为DeclRefExpr返回地址）
      mCurIrb->CreateStore(rhs, lhs);
      return rhs;
    }
    case BinaryExpr::kGt:
      return mCurIrb->CreateICmpSGT(lhs, rhs, "cmp");
    case BinaryExpr::kLt:
      return mCurIrb->CreateICmpSLT(lhs, rhs, "cmp");
    case BinaryExpr::kGe:
      return mCurIrb->CreateICmpSGE(lhs, rhs, "cmp");
    case BinaryExpr::kLe:
      return mCurIrb->CreateICmpSLE(lhs, rhs, "cmp");
    case BinaryExpr::kEq:
      return mCurIrb->CreateICmpEQ(lhs, rhs, "cmp");
    case BinaryExpr::kNe:
      return mCurIrb->CreateICmpNE(lhs, rhs, "cmp");
    case BinaryExpr::kAnd: {
      // 短路求值：lhs && rhs
      // 如果lhs为假，直接返回0；否则计算rhs
      llvm::Function* func = mCurIrb->GetInsertBlock()->getParent();
      llvm::BasicBlock* rhsBB = llvm::BasicBlock::Create(mCtx, "and.rhs", func);
      llvm::BasicBlock* falseBB = llvm::BasicBlock::Create(mCtx, "and.false", func);
      llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(mCtx, "and.end", func);
      
      // 将lhs转换为i1类型（如果还不是的话）
      llvm::Value* lhsIsTrue;
      if (lhs->getType()->isIntegerTy(1)) {
        lhsIsTrue = lhs;
      } else {
        lhsIsTrue = mCurIrb->CreateICmpNE(lhs, llvm::ConstantInt::get(lhs->getType(), 0), "lhs.bool");
      }
      mCurIrb->CreateCondBr(lhsIsTrue, rhsBB, falseBB);
      
      // lhs为假的情况
      mCurIrb->SetInsertPoint(falseBB);
      mCurIrb->CreateBr(mergeBB);
      
      // 计算rhs
      mCurIrb->SetInsertPoint(rhsBB);
      llvm::Value* rhsVal = self(obj->rht);
      mCurIrb->CreateBr(mergeBB);
      
      // 合并
      mCurIrb->SetInsertPoint(mergeBB);
      llvm::PHINode* phi = mCurIrb->CreatePHI(mIntTy, 2, "and.result");
      phi->addIncoming(llvm::ConstantInt::get(mIntTy, 0), falseBB);
      phi->addIncoming(rhsVal, rhsBB);
      return phi;
    }
    case BinaryExpr::kOr: {
      // 短路求值：lhs || rhs
      // 如果lhs为真，直接返回1；否则计算rhs
      llvm::Function* func = mCurIrb->GetInsertBlock()->getParent();
      llvm::BasicBlock* rhsBB = llvm::BasicBlock::Create(mCtx, "or.rhs", func);
      llvm::BasicBlock* trueBB = llvm::BasicBlock::Create(mCtx, "or.true", func);
      llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(mCtx, "or.end", func);
      
      // 将lhs转换为i1类型（如果还不是的话）
      llvm::Value* lhsIsTrue;
      if (lhs->getType()->isIntegerTy(1)) {
        lhsIsTrue = lhs;
      } else {
        lhsIsTrue = mCurIrb->CreateICmpNE(lhs, llvm::ConstantInt::get(lhs->getType(), 0), "lhs.bool");
      }
      mCurIrb->CreateCondBr(lhsIsTrue, trueBB, rhsBB);
      
      // lhs为真的情况
      mCurIrb->SetInsertPoint(trueBB);
      mCurIrb->CreateBr(mergeBB);
      
      // 计算rhs
      mCurIrb->SetInsertPoint(rhsBB);
      llvm::Value* rhsVal = self(obj->rht);
      mCurIrb->CreateBr(mergeBB);
      
      // 合并
      mCurIrb->SetInsertPoint(mergeBB);
      llvm::PHINode* phi = mCurIrb->CreatePHI(mIntTy, 2, "or.result");
      phi->addIncoming(llvm::ConstantInt::get(mIntTy, 1), trueBB);
      phi->addIncoming(rhsVal, rhsBB);
      return phi;
    }
    case BinaryExpr::kIndex: {
      // 数组索引：lft可能是数组类型或指针类型，rht是索引
      // 使用obj->type获取元素类型（索引表达式的类型是元素类型）
      llvm::Type* elemType = self(obj->type);
      
      // 如果lhs是数组类型，需要先转换为指针
      llvm::Value* arrPtr = lhs;
      if (lhs->getType()->isArrayTy()) {
        // 对于数组类型，创建GEP来获取首元素地址
        llvm::Value* zero = llvm::ConstantInt::get(mIntTy, 0);
        arrPtr = mCurIrb->CreateInBoundsGEP(lhs->getType(), lhs, zero);
      }
      // 如果lhs已经是指针类型，直接使用
      
      return mCurIrb->CreateInBoundsGEP(elemType, arrPtr, rhs, "idx");
    }
    default:
      ABORT();
  }
}

llvm::Value*
EmitIR::operator()(ImplicitCastExpr* obj)
{
  llvm::Value* sub = self(obj->sub);
  
  switch (obj->kind) {
    case ImplicitCastExpr::kLValueToRValue:
      // 从地址Load值
      return mCurIrb->CreateLoad(self(obj->type), sub, "l2r");
    case ImplicitCastExpr::kArrayToPointerDecay:
      // 数组到指针的decay
      if (sub->getType()->isArrayTy()) {
        llvm::Type* elemType = sub->getType()->getArrayElementType();
        llvm::Value* zero = llvm::ConstantInt::get(mIntTy, 0);
        return mCurIrb->CreateInBoundsGEP(elemType, sub, zero);
      }
      return sub;
    case ImplicitCastExpr::kFunctionToPointerDecay:
    case ImplicitCastExpr::kIntegralCast:
    case ImplicitCastExpr::kNoOp:
      return sub;
    default:
      ABORT();
  }
}

llvm::Value*
EmitIR::operator()(UnaryExpr* obj)
{
  llvm::Value* sub = self(obj->sub);
  switch (obj->op) {
    case UnaryExpr::kNeg:
      return mCurIrb->CreateNeg(sub, "neg");
    case UnaryExpr::kPos:
      return sub;
    case UnaryExpr::kNot: {
      // 逻辑非：与0比较得到i1类型，直接返回i1
      llvm::Value* zero = llvm::ConstantInt::get(sub->getType(), 0);
      return mCurIrb->CreateICmpEQ(sub, zero, "lnot");
    }
    default:
      ABORT();
  }
}

llvm::Value*
EmitIR::operator()(CallExpr* obj)
{
  // 获取被调用的函数 - head可能是ImplicitCastExpr包裹DeclRefExpr
  Expr* head = obj->head;
  auto implicitCast = head->dcst<ImplicitCastExpr>();
  if (implicitCast) {
    head = implicitCast->sub;
  }
  auto funcDecl = head->dcst<DeclRefExpr>();
  if (!funcDecl) {
    ABORT();
  }
  auto func = static_cast<llvm::Function*>(funcDecl->decl->any);
  if (!func) {
    ABORT();
  }
  
  // 收集参数
  std::vector<llvm::Value*> args;
  for (auto&& arg : obj->args) {
    args.push_back(self(arg));
  }
  
  // 如果返回void类型，不能给Call指令命名
  if (func->getReturnType()->isVoidTy()) {
    return mCurIrb->CreateCall(func, args);
  }
  return mCurIrb->CreateCall(func, args, "call");
}

llvm::Value*
EmitIR::operator()(ParenExpr* obj)
{
  return self(obj->sub);
}

llvm::Value*
EmitIR::operator()(InitListExpr* obj)
{
  // 初始化列表表达式，用于数组初始化
  // 需要生成llvm::ConstantAggregate或llvm::ConstantArray
  std::vector<llvm::Constant*> elements;
  
  for (auto&& elem : obj->list) {
    llvm::Value* val = self(elem);
    if (auto constant = llvm::dyn_cast<llvm::Constant>(val)) {
      elements.push_back(constant);
    } else {
      // 如果不是常量，说明是运行时表达式，需要特殊处理
      ABORT();
    }
  }
  
  // 获取数组类型
  llvm::Type* arrType = self(obj->type);
  if (auto arrTy = llvm::dyn_cast<llvm::ArrayType>(arrType)) {
    return llvm::ConstantArray::get(arrTy, elements);
  }
  
  ABORT();
}

llvm::Value*
EmitIR::operator()(ImplicitInitExpr* obj)
{
  // 隐式初始化，返回零初始化常量
  llvm::Type* ty = self(obj->type);
  return llvm::Constant::getNullValue(ty);
}

//==============================================================================
// 语句
//==============================================================================

void
EmitIR::operator()(Stmt* obj)
{
  if (auto p = obj->dcst<CompoundStmt>())
    return self(p);

  if (auto p = obj->dcst<ReturnStmt>())
    return self(p);

  if (auto p = obj->dcst<DeclStmt>())
    return self(p);

  if (auto p = obj->dcst<ExprStmt>())
    return self(p);

  if (auto p = obj->dcst<IfStmt>())
    return self(p);

  if (auto p = obj->dcst<WhileStmt>())
    return self(p);

  if (auto p = obj->dcst<DoStmt>())
    return self(p);

  if (auto p = obj->dcst<BreakStmt>())
    return self(p);

  if (auto p = obj->dcst<ContinueStmt>())
    return self(p);

  if (auto p = obj->dcst<NullStmt>())
    return self(p);

  ABORT();
}

void
EmitIR::operator()(CompoundStmt* obj)
{
  for (auto&& stmt : obj->subs)
    self(stmt);
}

void
EmitIR::operator()(DeclStmt* obj)
{
  for (auto&& decl : obj->decls)
    self(decl);
}

void
EmitIR::operator()(ExprStmt* obj)
{
  self(obj->expr);
}

void
EmitIR::operator()(ReturnStmt* obj)
{
  llvm::Value* retVal;
  if (!obj->expr)
    retVal = nullptr;
  else
    retVal = self(obj->expr);

  mCurIrb->CreateRet(retVal);
}

void
EmitIR::operator()(IfStmt* obj)
{
  llvm::Value* cond = self(obj->cond);
  
  // 如果cond不是i1类型，转换为i1
  if (!cond->getType()->isIntegerTy(1)) {
    cond = mCurIrb->CreateICmpNE(cond, llvm::ConstantInt::get(cond->getType(), 0), "cond.bool");
  }
  
  llvm::Function* func = mCurIrb->GetInsertBlock()->getParent();
  llvm::BasicBlock* thenBB = llvm::BasicBlock::Create(mCtx, "if.then", func);
  llvm::BasicBlock* elseBB = llvm::BasicBlock::Create(mCtx, "if.else");
  llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(mCtx, "if.end", func);
  
  // 判断是否有else分支
  if (obj->else_) {
    elseBB->insertInto(func);
    mCurIrb->CreateCondBr(cond, thenBB, elseBB);
    
    // then块
    mCurIrb->SetInsertPoint(thenBB);
    self(obj->then);
    if (!mCurIrb->GetInsertBlock()->getTerminator()) {
      mCurIrb->CreateBr(mergeBB);
    }
    
    // else块
    mCurIrb->SetInsertPoint(elseBB);
    self(obj->else_);
    if (!mCurIrb->GetInsertBlock()->getTerminator()) {
      mCurIrb->CreateBr(mergeBB);
    }
  } else {
    mCurIrb->CreateCondBr(cond, thenBB, mergeBB);
    
    // then块
    mCurIrb->SetInsertPoint(thenBB);
    self(obj->then);
    if (!mCurIrb->GetInsertBlock()->getTerminator()) {
      mCurIrb->CreateBr(mergeBB);
    }
  }
  
  // 合并块
  mCurIrb->SetInsertPoint(mergeBB);
}

void
EmitIR::operator()(WhileStmt* obj)
{
  llvm::Function* func = mCurIrb->GetInsertBlock()->getParent();
  llvm::BasicBlock* condBB = llvm::BasicBlock::Create(mCtx, "while.cond", func);
  llvm::BasicBlock* bodyBB = llvm::BasicBlock::Create(mCtx, "while.body", func);
  llvm::BasicBlock* exitBB = llvm::BasicBlock::Create(mCtx, "while.end", func);
  
  // 跳转到条件块
  mCurIrb->CreateBr(condBB);
  
  // 条件块
  mCurIrb->SetInsertPoint(condBB);
  llvm::Value* cond = self(obj->cond);
  // 如果cond不是i1类型，转换为i1
  if (!cond->getType()->isIntegerTy(1)) {
    cond = mCurIrb->CreateICmpNE(cond, llvm::ConstantInt::get(cond->getType(), 0), "cond.bool");
  }
  mCurIrb->CreateCondBr(cond, bodyBB, exitBB);
  
  // 循环体
  mCurIrb->SetInsertPoint(bodyBB);
  llvm::BasicBlock* savedLoopCond = mCurLoopCond;
  llvm::BasicBlock* savedLoopExit = mCurLoopExit;
  mCurLoopCond = condBB;
  mCurLoopExit = exitBB;
  self(obj->body);
  mCurLoopCond = savedLoopCond;
  mCurLoopExit = savedLoopExit;
  if (!mCurIrb->GetInsertBlock()->getTerminator()) {
    mCurIrb->CreateBr(condBB);
  }
  
  // 退出块
  mCurIrb->SetInsertPoint(exitBB);
}

void
EmitIR::operator()(DoStmt* obj)
{
  llvm::Function* func = mCurIrb->GetInsertBlock()->getParent();
  llvm::BasicBlock* bodyBB = llvm::BasicBlock::Create(mCtx, "do.body", func);
  llvm::BasicBlock* condBB = llvm::BasicBlock::Create(mCtx, "do.cond", func);
  llvm::BasicBlock* exitBB = llvm::BasicBlock::Create(mCtx, "do.end", func);
  
  // 跳转到循环体
  mCurIrb->CreateBr(bodyBB);
  
  // 循环体
  mCurIrb->SetInsertPoint(bodyBB);
  llvm::BasicBlock* savedLoopCond = mCurLoopCond;
  llvm::BasicBlock* savedLoopExit = mCurLoopExit;
  mCurLoopCond = condBB;
  mCurLoopExit = exitBB;
  self(obj->body);
  mCurLoopCond = savedLoopCond;
  mCurLoopExit = savedLoopExit;
  if (!mCurIrb->GetInsertBlock()->getTerminator()) {
    mCurIrb->CreateBr(condBB);
  }
  
  // 条件块
  mCurIrb->SetInsertPoint(condBB);
  llvm::Value* cond = self(obj->cond);
  mCurIrb->CreateCondBr(cond, bodyBB, exitBB);
  
  // 退出块
  mCurIrb->SetInsertPoint(exitBB);
}

void
EmitIR::operator()(BreakStmt* obj)
{
  if (!mCurLoopExit) {
    ABORT();
  }
  mCurIrb->CreateBr(mCurLoopExit);
}

void
EmitIR::operator()(ContinueStmt* obj)
{
  if (!mCurLoopCond) {
    ABORT();
  }
  mCurIrb->CreateBr(mCurLoopCond);
}

void
EmitIR::operator()(NullStmt* obj)
{
  // 空语句，什么都不做
}

//==============================================================================
// 声明
//==============================================================================

void EmitIR::operator()(Decl* obj) {
  if (auto p = obj->dcst<FunctionDecl>())
    return self(p);
  
  if (auto p = obj->dcst<VarDecl>())
    return self(p);
  
  ABORT();
}

void EmitIR::operator()(VarDecl* obj) {
    if (mCurFunc) {
        // 局部变量：在栈上分配
        auto alloca = mCurIrb->CreateAlloca(self(obj->type), nullptr, obj->name);
        obj->any = alloca;
        
        // 如果有初始化表达式
        if (obj->init) {
            // 检查是否是InitListExpr（包括空的初始化列表 ={}）
            if (auto initList = obj->init->dcst<asg::InitListExpr>()) {
                // 数组初始化列表，需要逐个元素存储
                initArray(alloca, obj->type, initList, 0);
            } else {
                llvm::Value* initVal = self(obj->init);
                // 检查是否是常量数组初始化
                if (auto constant = llvm::dyn_cast<llvm::Constant>(initVal)) {
                    // 对于数组类型，需要特殊处理
                    if (obj->type->texp && obj->type->texp->dcst<asg::ArrayType>()) {
                        // 创建全局常量数组，然后memcpy到栈上
                        std::string globalName = obj->name + ".init";
                        auto globalInit = new llvm::GlobalVariable(
                            mMod, constant->getType(), true,
                            llvm::GlobalValue::PrivateLinkage, constant, globalName);
                        
                        // 生成memcpy
                        llvm::Value* destPtr = alloca;
                        llvm::Value* srcPtr = globalInit;
                        llvm::Type* elemType = self(obj->type);
                        if (auto arrTy = llvm::dyn_cast<llvm::ArrayType>(elemType)) {
                            llvm::Value* size = llvm::ConstantInt::get(
                                mIntTy, arrTy->getNumElements() * 4);
                            mCurIrb->CreateMemCpy(
                                destPtr, llvm::MaybeAlign(4),
                                srcPtr, llvm::MaybeAlign(4),
                                size);
                        }
                    } else {
                        mCurIrb->CreateStore(constant, alloca);
                    }
                } else {
                    mCurIrb->CreateStore(initVal, alloca);
                }
            }
        }
    } else {
        // 全局变量处理
        llvm::Constant* init = nullptr;
        
        if (obj->init) {
            llvm::Value* initVal = self(obj->init);
            if (auto constant = llvm::dyn_cast<llvm::Constant>(initVal)) {
                init = constant;
            } else {
                ABORT();
            }
        } else {
            init = llvm::Constant::getNullValue(self(obj->type));
        }
        
        auto linkage = llvm::GlobalValue::ExternalLinkage;
        auto gv = new llvm::GlobalVariable(
            mMod, self(obj->type), obj->type->qual.const_,
            linkage, init, obj->name);
        
        obj->any = gv;
    }
}

void
EmitIR::operator()(FunctionDecl* obj)
{
  // 创建函数
  auto fty = llvm::dyn_cast<llvm::FunctionType>(self(obj->type));
  auto func = llvm::Function::Create(
    fty, llvm::GlobalVariable::ExternalLinkage, obj->name, mMod);

  obj->any = func;

  if (obj->body == nullptr) {
    return;
  }
  auto entryBb = llvm::BasicBlock::Create(mCtx, "entry", func);
  mCurIrb->SetInsertPoint(entryBb);

  // 处理函数参数
  unsigned idx = 0;
  for (auto& arg : func->args()) {
    if (idx < obj->params.size()) {
      auto param = obj->params[idx];
      // 为参数分配栈空间
      auto alloca = mCurIrb->CreateAlloca(self(param->type), nullptr, param->name);
      mCurIrb->CreateStore(&arg, alloca);
      param->any = alloca;
      idx++;
    }
  }

  // 翻译函数体
  mCurFunc = func;
  self(obj->body);
  auto& exitIrb = *mCurIrb;

  // 检查当前基本块是否已经有终结指令
  auto* curBB = exitIrb.GetInsertBlock();
  if (!curBB->getTerminator()) {
    if (fty->getReturnType()->isVoidTy())
      exitIrb.CreateRetVoid();
    else
      exitIrb.CreateUnreachable();
  }
}

void
EmitIR::initArray(llvm::Value* alloca, const asg::Type* type, asg::InitListExpr* list, int depth)
{
  // 获取当前维度的数组类型
  auto arrayType = type->texp->dcst<asg::ArrayType>();
  if (!arrayType) {
    // 不是数组类型，直接存储值
    if (list->list.empty()) {
      // 空初始化列表，存储零
      llvm::Type* ty = self(type);
      llvm::Value* zero = llvm::Constant::getNullValue(ty);
      mCurIrb->CreateStore(zero, alloca);
    } else {
      llvm::Value* val = self(list->list[0]);
      mCurIrb->CreateStore(val, alloca);
    }
    return;
  }
  
  // 计算子类型
  asg::Type subType;
  subType.spec = type->spec;
  subType.qual = type->qual;
  subType.texp = arrayType->sub;
  
  llvm::Type* elemType = self(&subType);
  
  // 如果初始化列表为空，用零填充整个数组
  if (list->list.empty()) {
    llvm::Value* zero = llvm::Constant::getNullValue(elemType);
    for (int i = 0; i < arrayType->len; ++i) {
      llvm::Value* idx = llvm::ConstantInt::get(mIntTy, i);
      llvm::Value* elemPtr = mCurIrb->CreateInBoundsGEP(elemType, alloca, idx);
      mCurIrb->CreateStore(zero, elemPtr);
    }
    return;
  }
  
  // 遍历初始化列表中的每个元素
  for (size_t i = 0; i < list->list.size(); ++i) {
    auto elem = list->list[i];
    
    // 计算当前元素的地址
    llvm::Value* idx = llvm::ConstantInt::get(mIntTy, i);
    llvm::Value* elemPtr = mCurIrb->CreateInBoundsGEP(elemType, alloca, idx);
    
    if (auto nestedList = elem->dcst<asg::InitListExpr>()) {
      // 嵌套的初始化列表，递归处理
      initArray(elemPtr, &subType, nestedList, depth + 1);
    } else if (auto implicitInit = elem->dcst<asg::ImplicitInitExpr>()) {
      // 隐式初始化，存储零
      llvm::Value* zero = llvm::Constant::getNullValue(elemType);
      mCurIrb->CreateStore(zero, elemPtr);
    } else {
      // 普通表达式，计算值并存储
      llvm::Value* val = self(elem);
      mCurIrb->CreateStore(val, elemPtr);
    }
  }
}
