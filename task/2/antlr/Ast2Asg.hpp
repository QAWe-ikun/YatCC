#pragma once

#include "SYsUParser.h"
#include "asg.hpp"

namespace asg {

using ast = SYsUParser;

class Ast2Asg
{
public:
  Obj::Mgr& mMgr;

  Ast2Asg(Obj::Mgr& mgr)
    : mMgr(mgr)
  {
  }

  TranslationUnit* operator()(ast::TranslationUnitContext* ctx);

  //============================================================================
  // 类型
  //============================================================================

  using SpecQual = std::pair<Type::Spec, Type::Qual>;

  SpecQual operator()(ast::DeclarationSpecifiersContext* ctx);

  // 基本版本，只返回类型表达式和名字
  std::pair<TypeExpr*, std::string> operator()(ast::DeclaratorContext* ctx,
                                               TypeExpr* sub);

  std::pair<TypeExpr*, std::string> operator()(
    ast::DirectDeclaratorContext* ctx,
    TypeExpr* sub);

  // 扩展版本，返回类型表达式、名字和参数列表（用于函数定义）
  std::tuple<TypeExpr*, std::string, std::vector<Decl*>> operator()(
    ast::DeclaratorContext* ctx,
    TypeExpr* sub,
    bool withParams);

  std::tuple<TypeExpr*, std::string, std::vector<Decl*>> operator()(
    ast::DirectDeclaratorContext* ctx,
    TypeExpr* sub,
    bool withParams);

  //============================================================================
  // 表达式
  //============================================================================

  Expr* operator()(ast::ExpressionContext* ctx);

  Expr* operator()(ast::AssignmentExpressionContext* ctx);

  Expr* operator()(ast::ConditionalExpressionContext* ctx);

  Expr* operator()(ast::LogicalOrExpressionContext* ctx);

  Expr* operator()(ast::LogicalAndExpressionContext* ctx);

  Expr* operator()(ast::InclusiveOrExpressionContext* ctx);

  Expr* operator()(ast::ExclusiveOrExpressionContext* ctx);

  Expr* operator()(ast::AndExpressionContext* ctx);

  Expr* operator()(ast::EqualityExpressionContext* ctx);

  Expr* operator()(ast::RelationalExpressionContext* ctx);

  Expr* operator()(ast::ShiftExpressionContext* ctx);

  Expr* operator()(ast::AdditiveExpressionContext* ctx);

  Expr* operator()(ast::MultiplicativeExpressionContext* ctx);

  Expr* operator()(ast::UnaryExpressionContext* ctx);

  Expr* operator()(ast::PostfixExpressionContext* ctx);

  Expr* operator()(ast::PrimaryExpressionContext* ctx);

  Expr* operator()(ast::InitializerContext* ctx);

  std::vector<Expr*> operator()(ast::ArgumentExpressionListContext* ctx);

  //============================================================================
  // 语句
  //============================================================================

  Stmt* operator()(ast::StatementContext* ctx);

  CompoundStmt* operator()(ast::CompoundStatementContext* ctx);

  Stmt* operator()(ast::ExpressionStatementContext* ctx);

  Stmt* operator()(ast::SelectionStatementContext* ctx);

  Stmt* operator()(ast::IterationStatementContext* ctx);

  Stmt* operator()(ast::JumpStatementContext* ctx);

  //============================================================================
  // 声明
  //============================================================================

  std::vector<Decl*> operator()(ast::DeclarationContext* ctx);

  FunctionDecl* operator()(ast::FunctionDefinitionContext* ctx);

  Decl* operator()(ast::InitDeclaratorContext* ctx, SpecQual sq);

  std::vector<Decl*> operator()(ast::ParameterListContext* ctx);

  Decl* operator()(ast::ParameterDeclarationContext* ctx);

private:
  struct Symtbl;
  Symtbl* mSymtbl{ nullptr };

  FunctionDecl* mCurrentFunc{ nullptr };

  Stmt* mCurrentLoop{ nullptr };  // 当前所在的循环语句

  template<typename T, typename... Args>
  T* make(Args... args)
  {
    return mMgr.make<T>(args...);
  }
};

} // namespace asg
