#include "EmitIR.hpp"
#include <llvm/Transforms/Utils/ModuleUtils.h>

#define self (*this)

using namespace asg;

EmitIR::EmitIR(Obj::Mgr& mgr, llvm::LLVMContext& ctx, llvm::StringRef mid)
  : mMgr(mgr)
  , mMod(mid, ctx)
  , mCtx(ctx)
  , mIntTy(llvm::Type::getInt32Ty(ctx))
  , mCurIrb(std::make_unique<llvm::IRBuilder<>>(ctx))
  , mCtorTy(llvm::FunctionType::get(llvm::Type::getVoidTy(ctx), false))
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
      case Type::Spec::kInt:
        return llvm::Type::getInt32Ty(mCtx);
      // TODO: 在此添加对更多基础类型的处理
      default:
        ABORT();
    }
  }

  Type subt;
  subt.spec = type->spec;
  subt.qual = type->qual;
  subt.texp = type->texp->sub;

  // TODO: 在此添加对指针类型、数组类型和函数类型的处理

  if (auto p = type->texp->dcst<FunctionType>()) {
    std::vector<llvm::Type*> pty;
    // TODO: 在此添加对函数参数类型的处理
    return llvm::FunctionType::get(self(&subt), std::move(pty), false);
  }

  ABORT();
}

//==============================================================================
// 表达式
//==============================================================================

llvm::Value* EmitIR::operator()(Expr* obj) {
  if (auto p = obj->dcst<IntegerLiteral>())
    return self(p);
  
  // 添加 DeclRefExpr 处理
  if (auto p = obj->dcst<DeclRefExpr>()) {
    if (auto var = p->decl->dcst<VarDecl>()) {
      // 从 var->any 获取分配的 Value
      llvm::Value* addr = static_cast<llvm::Value*>(var->any);
      return mCurIrb->CreateLoad(self(var->type), addr);
    }
  }
  
  ABORT();
}

llvm::Constant*
EmitIR::operator()(IntegerLiteral* obj)
{
  return llvm::ConstantInt::get(self(obj->type), obj->val);
}

// TODO: 在此添加对更多表达式类型的处理

//==============================================================================
// 语句
//==============================================================================

void
EmitIR::operator()(Stmt* obj)
{
  // TODO: 在此添加对更多Stmt类型的处理的跳转

  if (auto p = obj->dcst<CompoundStmt>())
    return self(p);

  if (auto p = obj->dcst<ReturnStmt>())
    return self(p);

  ABORT();
}

// TODO: 在此添加对更多Stmt类型的处理

void
EmitIR::operator()(CompoundStmt* obj)
{
  // TODO: 可以在此添加对符号重名的处理
  for (auto&& stmt : obj->subs)
    self(stmt);
}

void
EmitIR::operator()(ReturnStmt* obj)
{
  auto& irb = *mCurIrb;

  llvm::Value* retVal;
  if (!obj->expr)
    retVal = nullptr;
  else
    retVal = self(obj->expr);

  mCurIrb->CreateRet(retVal);

  auto exitBb = llvm::BasicBlock::Create(mCtx, "return_exit", mCurFunc);
  mCurIrb->SetInsertPoint(exitBb);
}

//==============================================================================
// 声明
//==============================================================================

void EmitIR::operator()(Decl* obj) {
  if (auto p = obj->dcst<FunctionDecl>())
    return self(p);
  
  // 添加 VarDecl 处理
  if (auto p = obj->dcst<VarDecl>()) {
    // 生成全局/局部变量
    return self(p);
  }
  
  ABORT();
}

// TODO: 添加变量声明的处理
void EmitIR::operator()(VarDecl* obj) {
    if (mCurFunc) {
        // 局部变量处理不变
    } else {
        // 全局变量处理
        llvm::Constant* init = nullptr;
        
        // 处理有初始化表达式的情况
        if (obj->init) {
            if (auto constant = llvm::dyn_cast<llvm::Constant>(self(obj->init))) {
                init = constant;
            } else {
                // 非常量初始化需要特殊处理（如运行时代码）
                ABORT(); // 简化处理，实际需添加全局构造函数
            }
        } else {
            // 无初始化表达式时，默认初始化为0
            init = llvm::Constant::getNullValue(self(obj->type));
        }
        
        // 修正链接类型：有初始值用 CommonLinkage
        auto linkage = init ? llvm::GlobalValue::CommonLinkage 
                            : llvm::GlobalValue::ExternalLinkage;
        
        auto gv = new llvm::GlobalVariable(
            mMod, self(obj->type), false, // 非常量
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

  if (obj->body == nullptr)
    return;
  auto entryBb = llvm::BasicBlock::Create(mCtx, "entry", func);
  mCurIrb->SetInsertPoint(entryBb);
  auto& entryIrb = *mCurIrb;

  // TODO: 添加对函数参数的处理

  // 翻译函数体
  mCurFunc = func;
  self(obj->body);
  auto& exitIrb = *mCurIrb;

  if (fty->getReturnType()->isVoidTy())
    exitIrb.CreateRetVoid();
  else
    exitIrb.CreateUnreachable();
}
