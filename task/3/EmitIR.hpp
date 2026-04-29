#include "asg.hpp"
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>

class EmitIR
{
public:
  Obj::Mgr& mMgr;
  llvm::Module mMod;

  EmitIR(Obj::Mgr& mgr, llvm::LLVMContext& ctx, llvm::StringRef mid = "-");

  llvm::Module& operator()(asg::TranslationUnit* tu);

private:
  llvm::LLVMContext& mCtx;

  llvm::Type* mIntTy;
  llvm::FunctionType* mCtorTy;

  llvm::Function* mCurFunc;
  std::unique_ptr<llvm::IRBuilder<>> mCurIrb;

  // 用于break/continue的循环控制
  llvm::BasicBlock* mCurLoopCond;
  llvm::BasicBlock* mCurLoopExit;

  //============================================================================
  // 类型
  //============================================================================

  llvm::Type* operator()(const asg::Type* type);

  //============================================================================
  // 表达式
  //============================================================================

  llvm::Value* operator()(asg::Expr* obj);

  llvm::Constant* operator()(asg::IntegerLiteral* obj);

  llvm::Value* operator()(asg::DeclRefExpr* obj);
  llvm::Value* operator()(asg::BinaryExpr* obj);
  llvm::Value* operator()(asg::ImplicitCastExpr* obj);
  llvm::Value* operator()(asg::UnaryExpr* obj);
  llvm::Value* operator()(asg::CallExpr* obj);
  llvm::Value* operator()(asg::ParenExpr* obj);
  llvm::Value* operator()(asg::InitListExpr* obj);
  llvm::Value* operator()(asg::ImplicitInitExpr* obj);

  //============================================================================
  // 语句
  //============================================================================

  void operator()(asg::Stmt* obj);

  void operator()(asg::CompoundStmt* obj);

  void operator()(asg::ReturnStmt* obj);
  void operator()(asg::DeclStmt* obj);
  void operator()(asg::ExprStmt* obj);
  void operator()(asg::IfStmt* obj);
  void operator()(asg::WhileStmt* obj);
  void operator()(asg::DoStmt* obj);
  void operator()(asg::BreakStmt* obj);
  void operator()(asg::ContinueStmt* obj);
  void operator()(asg::NullStmt* obj);

  //============================================================================
  // 声明
  //============================================================================

  void operator()(asg::Decl* obj);

  void operator()(asg::FunctionDecl* obj);

  void operator()(asg::VarDecl* obj);
  
  // 辅助函数：递归初始化数组
  void initArray(llvm::Value* alloca, const asg::Type* type, asg::InitListExpr* list, int depth);
};
