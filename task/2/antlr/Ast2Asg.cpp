#include "Ast2Asg.hpp"
#include <unordered_map>
#include <iostream>

#define self (*this)

namespace asg {

// 符号表，保存当前作用域的所有声明
struct Ast2Asg::Symtbl : public std::unordered_map<std::string, Decl*>
{
  Ast2Asg& m;
  Symtbl* mPrev;

  Symtbl(Ast2Asg& m)
    : m(m)
    , mPrev(m.mSymtbl)
  {
    m.mSymtbl = this;
  }

  ~Symtbl() { m.mSymtbl = mPrev; }

  Decl* resolve(const std::string& name);
};

Decl*
Ast2Asg::Symtbl::resolve(const std::string& name)
{
  auto iter = find(name);
  if (iter != end())
    return iter->second;
  if (mPrev == nullptr)
    std::cout << name << std::endl;
  ASSERT(mPrev != nullptr); // 标识符未定义
  return mPrev->resolve(name);
}

TranslationUnit*
Ast2Asg::operator()(ast::TranslationUnitContext* ctx)
{
  auto ret = make<asg::TranslationUnit>();
  if (ctx == nullptr)
    return ret;

  Symtbl localDecls(self);

  for (auto&& i : ctx->externalDeclaration()) {
    if (auto p = i->declaration()) {
      auto decls = self(p);
      for (auto&& decl : decls) {
        // 添加到符号表
        if (auto vdecl = decl->dcst<VarDecl>())
          localDecls[vdecl->name] = vdecl;
        else if (auto fdecl = decl->dcst<FunctionDecl>())
          localDecls[fdecl->name] = fdecl;
        ret->decls.push_back(decl);
      }
    }

    else if (auto p = i->functionDefinition()) {
      auto funcDecl = self(p);
      ret->decls.push_back(funcDecl);

      // 添加到声明表
      localDecls[funcDecl->name] = funcDecl;
    }

    else
      ABORT();
  }

  return ret;
}

//==============================================================================
// 类型
//==============================================================================

Ast2Asg::SpecQual
Ast2Asg::operator()(ast::DeclarationSpecifiersContext* ctx)
{
  SpecQual ret = { Type::Spec::kINVALID, Type::Qual() };

  for (auto&& i : ctx->declarationSpecifier()) {
    if (auto p = i->typeSpecifier()) {
      if (ret.first == Type::Spec::kINVALID) {
        if (p->Int())
          ret.first = Type::Spec::kInt;
        else if (p->Void())
          ret.first = Type::Spec::kVoid;
        else if (p->Char())
          ret.first = Type::Spec::kChar;
        else if (p->Long().size() == 2)  // long long
          ret.first = Type::Spec::kLongLong;
        else if (p->Long().size() == 1)  // long
          ret.first = Type::Spec::kLong;
        else
          ABORT(); // 未知的类型说明符
      }
      else if (ret.first == Type::Spec::kLong && p->Long().size() == 1) {
        // long long 的情况：第一个 long 已处理，第二个 long
        ret.first = Type::Spec::kLongLong;
      }
      else
        ABORT(); // 重复的类型说明符
    }
    else if (auto q = i->typeQualifier()) {
      // 处理类型限定符（const）
      if (q->Const())
        ret.second.const_ = true;
      else
        ABORT(); // 未知的类型限定符
    }
    else
      ABORT();
  }

  return ret;
}

std::pair<TypeExpr*, std::string>
Ast2Asg::operator()(ast::DeclaratorContext* ctx, TypeExpr* sub)
{
  return self(ctx->directDeclarator(), sub);
}

std::tuple<TypeExpr*, std::string, std::vector<Decl*>>
Ast2Asg::operator()(ast::DeclaratorContext* ctx, TypeExpr* sub, bool withParams)
{
  return self(ctx->directDeclarator(), sub, withParams);
}

static int
eval_arrlen(Expr* expr)
{
  if (auto p = expr->dcst<IntegerLiteral>())
    return p->val;

  if (auto p = expr->dcst<DeclRefExpr>()) {
    if (p->decl == nullptr)
      ABORT();

    auto var = p->decl->dcst<VarDecl>();
    if (!var || !var->type->qual.const_)
      ABORT(); // 数组长度必须是编译期常量

    switch (var->type->spec) {
      case Type::Spec::kChar:
      case Type::Spec::kInt:
      case Type::Spec::kLong:
      case Type::Spec::kLongLong:
        return eval_arrlen(var->init);

      default:
        ABORT(); // 长度表达式必须是数值类型
    }
  }

  if (auto p = expr->dcst<UnaryExpr>()) {
    auto sub = eval_arrlen(p->sub);

    switch (p->op) {
      case UnaryExpr::kPos:
        return sub;

      case UnaryExpr::kNeg:
        return -sub;

      default:
        ABORT();
    }
  }

  if (auto p = expr->dcst<BinaryExpr>()) {
    auto lft = eval_arrlen(p->lft);
    auto rht = eval_arrlen(p->rht);

    switch (p->op) {
      case BinaryExpr::kAdd:
        return lft + rht;

      case BinaryExpr::kSub:
        return lft - rht;

      default:
        ABORT();
    }
  }

  if (auto p = expr->dcst<InitListExpr>()) {
    if (p->list.empty())
      return 0;
    return eval_arrlen(p->list[0]);
  }

  ABORT();
}

std::pair<TypeExpr*, std::string>
Ast2Asg::operator()(ast::DirectDeclaratorContext* ctx, TypeExpr* sub)
{
  if (auto p = ctx->Identifier())
    return { sub, p->getText() };

  if (ctx->LeftBracket()) {
    auto arrayType = make<ArrayType>();
    arrayType->sub = sub;

    if (auto p = ctx->assignmentExpression())
      arrayType->len = eval_arrlen(self(p));
    else
      arrayType->len = ArrayType::kUnLen;

    return self(ctx->directDeclarator(), arrayType);
  }

  if (ctx->LeftParen()) {
    auto funcType = make<FunctionType>();
    funcType->sub = sub;

    if (auto p = ctx->parameterTypeList()) {
      auto params = self(p->parameterList());
      for (auto&& param : params) {
        if (auto varDecl = param->dcst<VarDecl>()) {
          funcType->params.push_back(varDecl->type);
        }
      }
    }

    return self(ctx->directDeclarator(), funcType);
  }

  ABORT();
}

std::tuple<TypeExpr*, std::string, std::vector<Decl*>>
Ast2Asg::operator()(ast::DirectDeclaratorContext* ctx, TypeExpr* sub, bool withParams)
{
  if (auto p = ctx->Identifier())
    return { sub, p->getText(), std::vector<Decl*>{} };

  if (ctx->LeftBracket()) {
    auto arrayType = make<ArrayType>();
    arrayType->sub = sub;

    if (auto p = ctx->assignmentExpression())
      arrayType->len = eval_arrlen(self(p));
    else
      arrayType->len = ArrayType::kUnLen;

    return self(ctx->directDeclarator(), arrayType, withParams);
  }

  if (ctx->LeftParen()) {
    auto funcType = make<FunctionType>();
    funcType->sub = sub;

    std::vector<Decl*> paramDecls;
    if (auto p = ctx->parameterTypeList()) {
      paramDecls = self(p->parameterList());
      for (auto&& param : paramDecls) {
        if (auto varDecl = param->dcst<VarDecl>()) {
          funcType->params.push_back(varDecl->type);
        }
      }
    }

    auto [texp, name, innerParams] = self(ctx->directDeclarator(), funcType, withParams);
    // 将当前参数添加到参数列表前面
    paramDecls.insert(paramDecls.end(), innerParams.begin(), innerParams.end());
    return { texp, name, paramDecls };
  }

  ABORT();
}

//==============================================================================
// 表达式
//==============================================================================

Expr*
Ast2Asg::operator()(ast::ExpressionContext* ctx)
{
  auto list = ctx->assignmentExpression();
  Expr* ret = self(list[0]);

  for (unsigned i = 1; i < list.size(); ++i) {
    auto node = make<BinaryExpr>();
    node->op = node->kComma;
    node->lft = ret;
    node->rht = self(list[i]);
    ret = node;
  }

  return ret;
}

Expr*
Ast2Asg::operator()(ast::AssignmentExpressionContext* ctx)
{
  if (auto p = ctx->conditionalExpression())
    return self(p);

  auto ret = make<BinaryExpr>();
  ret->op = ret->kAssign;
  ret->lft = self(ctx->unaryExpression());
  ret->rht = self(ctx->assignmentExpression());
  return ret;
}

Expr*
Ast2Asg::operator()(ast::ConditionalExpressionContext* ctx)
{
  auto ret = self(ctx->logicalOrExpression());

  if (ctx->Question()) {
    // 条件表达式 - 暂时简化处理
    // TODO: 实现完整的三目运算符支持
    ABORT();
  }

  return ret;
}

Expr*
Ast2Asg::operator()(ast::LogicalOrExpressionContext* ctx)
{
  auto children = ctx->logicalAndExpression();
  Expr* ret = self(children[0]);

  for (unsigned i = 1; i < children.size(); ++i) {
    auto node = make<BinaryExpr>();
    node->op = node->kOr;
    node->lft = ret;
    node->rht = self(children[i]);
    ret = node;
  }

  return ret;
}

Expr*
Ast2Asg::operator()(ast::LogicalAndExpressionContext* ctx)
{
  auto children = ctx->inclusiveOrExpression();
  Expr* ret = self(children[0]);

  for (unsigned i = 1; i < children.size(); ++i) {
    auto node = make<BinaryExpr>();
    node->op = node->kAnd;
    node->lft = ret;
    node->rht = self(children[i]);
    ret = node;
  }

  return ret;
}

Expr*
Ast2Asg::operator()(ast::InclusiveOrExpressionContext* ctx)
{
  auto children = ctx->exclusiveOrExpression();
  Expr* ret = self(children[0]);

  for (unsigned i = 1; i < children.size(); ++i) {
    auto node = make<BinaryExpr>();
    // 按位或操作 - 暂时用 kOr 表示
    node->op = node->kOr;
    node->lft = ret;
    node->rht = self(children[i]);
    ret = node;
  }

  return ret;
}

Expr*
Ast2Asg::operator()(ast::ExclusiveOrExpressionContext* ctx)
{
  auto children = ctx->andExpression();
  Expr* ret = self(children[0]);

  for (unsigned i = 1; i < children.size(); ++i) {
    auto node = make<BinaryExpr>();
    // 异或操作 - 暂不支持
    ABORT();
  }

  return ret;
}

Expr*
Ast2Asg::operator()(ast::AndExpressionContext* ctx)
{
  auto children = ctx->equalityExpression();
  Expr* ret = self(children[0]);

  for (unsigned i = 1; i < children.size(); ++i) {
    auto node = make<BinaryExpr>();
    // 按位与操作 - 暂不支持
    ABORT();
  }

  return ret;
}

Expr*
Ast2Asg::operator()(ast::EqualityExpressionContext* ctx)
{
  auto children = ctx->children;
  Expr* ret = self(dynamic_cast<ast::RelationalExpressionContext*>(children[0]));

  for (unsigned i = 1; i < children.size(); i += 2) {
    auto node = make<BinaryExpr>();

    auto token = dynamic_cast<antlr4::tree::TerminalNode*>(children[i])
                   ->getSymbol()
                   ->getType();
    switch (token) {
      case ast::EqualEqual:
        node->op = node->kEq;
        break;

      case ast::ExclaimEqual:
        node->op = node->kNe;
        break;

      default:
        ABORT();
    }

    node->lft = ret;
    node->rht = self(dynamic_cast<ast::RelationalExpressionContext*>(children[i + 1]));
    ret = node;
  }

  return ret;
}

Expr*
Ast2Asg::operator()(ast::RelationalExpressionContext* ctx)
{
  auto children = ctx->children;
  Expr* ret = self(dynamic_cast<ast::ShiftExpressionContext*>(children[0]));

  for (unsigned i = 1; i < children.size(); i += 2) {
    auto node = make<BinaryExpr>();

    auto token = dynamic_cast<antlr4::tree::TerminalNode*>(children[i])
                   ->getSymbol()
                   ->getType();
    switch (token) {
      case ast::Less:
        node->op = node->kLt;
        break;

      case ast::Greater:
        node->op = node->kGt;
        break;

      case ast::LessEqual:
        node->op = node->kLe;
        break;

      case ast::GreaterEqual:
        node->op = node->kGe;
        break;

      default:
        ABORT();
    }

    node->lft = ret;
    node->rht = self(dynamic_cast<ast::ShiftExpressionContext*>(children[i + 1]));
    ret = node;
  }

  return ret;
}

Expr*
Ast2Asg::operator()(ast::ShiftExpressionContext* ctx)
{
  auto children = ctx->additiveExpression();
  Expr* ret = self(children[0]);

  for (unsigned i = 1; i < children.size(); ++i) {
    // 移位操作 - 暂不支持
    ABORT();
  }

  return ret;
}

Expr*
Ast2Asg::operator()(ast::AdditiveExpressionContext* ctx)
{
  auto children = ctx->children;
  Expr* ret = self(dynamic_cast<ast::MultiplicativeExpressionContext*>(children[0]));

  for (unsigned i = 1; i < children.size(); i += 2) {
    auto node = make<BinaryExpr>();

    auto token = dynamic_cast<antlr4::tree::TerminalNode*>(children[i])
                   ->getSymbol()
                   ->getType();
    switch (token) {
      case ast::Plus:
        node->op = node->kAdd;
        break;

      case ast::Minus:
        node->op = node->kSub;
        break;

      default:
        ABORT();
    }

    node->lft = ret;
    node->rht = self(dynamic_cast<ast::MultiplicativeExpressionContext*>(children[i + 1]));
    ret = node;
  }

  return ret;
}

Expr*
Ast2Asg::operator()(ast::MultiplicativeExpressionContext* ctx)
{
  auto children = ctx->children;
  Expr* ret = self(dynamic_cast<ast::UnaryExpressionContext*>(children[0]));

  for (unsigned i = 1; i < children.size(); i += 2) {
    auto node = make<BinaryExpr>();

    auto token = dynamic_cast<antlr4::tree::TerminalNode*>(children[i])
                   ->getSymbol()
                   ->getType();
    switch (token) {
      case ast::Star:
        node->op = node->kMul;
        break;

      case ast::Slash:
        node->op = node->kDiv;
        break;

      case ast::Percent:
        node->op = node->kMod;
        break;

      default:
        ABORT();
    }

    node->lft = ret;
    node->rht = self(dynamic_cast<ast::UnaryExpressionContext*>(children[i + 1]));
    ret = node;
  }

  return ret;
}

Expr*
Ast2Asg::operator()(ast::UnaryExpressionContext* ctx)
{
  if (auto p = ctx->postfixExpression())
    return self(p);

  if (ctx->PlusPlus()) {
    // 前置++: ++a 转换为 a = a + 1
    auto sub = self(ctx->unaryExpression());
    auto one = make<IntegerLiteral>();
    one->val = 1;
    
    auto add = make<BinaryExpr>();
    add->op = add->kAdd;
    add->lft = sub;
    add->rht = one;
    
    auto assign = make<BinaryExpr>();
    assign->op = assign->kAssign;
    assign->lft = sub;
    assign->rht = add;
    return assign;
  }

  if (ctx->MinusMinus()) {
    // 前置--: --a 转换为 a = a - 1
    auto sub = self(ctx->unaryExpression());
    auto one = make<IntegerLiteral>();
    one->val = 1;
    
    auto subExpr = make<BinaryExpr>();
    subExpr->op = subExpr->kSub;
    subExpr->lft = sub;
    subExpr->rht = one;
    
    auto assign = make<BinaryExpr>();
    assign->op = assign->kAssign;
    assign->lft = sub;
    assign->rht = subExpr;
    return assign;
  }

  if (auto p = ctx->unaryOperator()) {
    auto ret = make<UnaryExpr>();

    auto token = dynamic_cast<antlr4::tree::TerminalNode*>(p->children[0])
                   ->getSymbol()
                   ->getType();
    switch (token) {
      case ast::Plus:
        ret->op = ret->kPos;
        break;

      case ast::Minus:
        ret->op = ret->kNeg;
        break;

      case ast::Exclaim:
        ret->op = ret->kNot;
        break;

      case ast::Tilde:
        // 按位取反 - 暂不支持
        ABORT();

      default:
        ABORT();
    }

    ret->sub = self(ctx->unaryExpression());
    return ret;
  }

  ABORT();
}

Expr*
Ast2Asg::operator()(ast::PostfixExpressionContext* ctx)
{
  auto children = ctx->children;

  if (children.size() == 1) {
    return self(dynamic_cast<ast::PrimaryExpressionContext*>(children[0]));
  }

  // 处理后缀表达式
  // children[0] 可能是 PrimaryExpressionContext 或 PostfixExpressionContext
  Expr* base = nullptr;
  if (auto primary = dynamic_cast<ast::PrimaryExpressionContext*>(children[0])) {
    base = self(primary);
  } else if (auto postfix = dynamic_cast<ast::PostfixExpressionContext*>(children[0])) {
    base = self(postfix);
  } else {
    ABORT();
  }

  for (unsigned i = 1; i < children.size(); ++i) {
    if (auto p = dynamic_cast<antlr4::tree::TerminalNode*>(children[i])) {
      auto token = p->getSymbol()->getType();

      if (token == ast::LeftBracket) {
        // 数组下标访问
        auto index = self(dynamic_cast<ast::ExpressionContext*>(children[i + 1]));
        auto node = make<BinaryExpr>();
        node->op = node->kIndex;
        node->lft = base;
        node->rht = index;
        base = node;
        i += 2; // 跳过 [ expression ]
      }
      else if (token == ast::LeftParen) {
        // 函数调用
        auto node = make<CallExpr>();
        node->head = base;

        if (auto args = dynamic_cast<ast::ArgumentExpressionListContext*>(children[i + 1])) {
          node->args = self(args);
          i += 2; // 跳过 ( args )
        }
        else {
          i += 1; // 跳过 (
        }

        base = node;
      }
      else if (token == ast::PlusPlus) {
        // 后置++: a++ 转换为 a = a + 1
        // 注意：这里简化处理，实际语义应该是返回原值再自增
        auto one = make<IntegerLiteral>();
        one->val = 1;
        
        auto add = make<BinaryExpr>();
        add->op = add->kAdd;
        add->lft = base;
        add->rht = one;
        
        auto assign = make<BinaryExpr>();
        assign->op = assign->kAssign;
        assign->lft = base;
        assign->rht = add;
        base = assign;
      }
      else if (token == ast::MinusMinus) {
        // 后置--: a-- 转换为 a = a - 1
        // 注意：这里简化处理，实际语义应该是返回原值再自减
        auto one = make<IntegerLiteral>();
        one->val = 1;
        
        auto subExpr = make<BinaryExpr>();
        subExpr->op = subExpr->kSub;
        subExpr->lft = base;
        subExpr->rht = one;
        
        auto assign = make<BinaryExpr>();
        assign->op = assign->kAssign;
        assign->lft = base;
        assign->rht = subExpr;
        base = assign;
      }
    }
  }

  return base;
}

Expr*
Ast2Asg::operator()(ast::PrimaryExpressionContext* ctx)
{
  if (auto p = ctx->Identifier()) {
    auto name = p->getText();
    auto ret = make<DeclRefExpr>();
    ret->decl = mSymtbl->resolve(name);
    return ret;
  }

  if (auto p = ctx->Constant()) {
    auto text = p->getText();

    auto ret = make<IntegerLiteral>();

    ASSERT(!text.empty());
    if (text[0] != '0')
      ret->val = std::stoll(text);

    else if (text.size() == 1)
      ret->val = 0;

    else if (text[1] == 'x' || text[1] == 'X')
      ret->val = std::stoll(text.substr(2), nullptr, 16);

    else
      ret->val = std::stoll(text.substr(1), nullptr, 8);

    return ret;
  }

  if (ctx->LeftParen()) {
    // 创建 ParenExpr 节点
    auto ret = make<ParenExpr>();
    ret->sub = self(ctx->expression());
    return ret;
  }

  ABORT();
}

Expr*
Ast2Asg::operator()(ast::InitializerContext* ctx)
{
  if (auto p = ctx->assignmentExpression())
    return self(p);

  auto ret = make<InitListExpr>();

  if (auto p = ctx->initializerList()) {
    for (auto&& i : p->initializer()) {
      // 将初始化列表展平
      auto expr = self(i);
      if (auto p = expr->dcst<InitListExpr>()) {
        for (auto&& sub : p->list)
          ret->list.push_back(sub);
      } else {
        ret->list.push_back(expr);
      }
    }
  }

  return ret;
}

std::vector<Expr*>
Ast2Asg::operator()(ast::ArgumentExpressionListContext* ctx)
{
  std::vector<Expr*> ret;
  for (auto&& i : ctx->assignmentExpression()) {
    ret.push_back(self(i));
  }
  return ret;
}

//==============================================================================
// 语句
//==============================================================================

Stmt*
Ast2Asg::operator()(ast::StatementContext* ctx)
{
  if (auto p = ctx->compoundStatement())
    return self(p);

  if (auto p = ctx->expressionStatement())
    return self(p);

  if (auto p = ctx->selectionStatement())
    return self(p);

  if (auto p = ctx->iterationStatement())
    return self(p);

  if (auto p = ctx->jumpStatement())
    return self(p);

  ABORT();
}

CompoundStmt*
Ast2Asg::operator()(ast::CompoundStatementContext* ctx)
{
  auto ret = make<CompoundStmt>();

  if (auto p = ctx->blockItemList()) {
    Symtbl localDecls(self);

    for (auto&& i : p->blockItem()) {
      if (auto q = i->declaration()) {
        auto sub = make<DeclStmt>();
        sub->decls = self(q);
        ret->subs.push_back(sub);
      }

      else if (auto q = i->statement())
        ret->subs.push_back(self(q));

      else
        ABORT();
    }
  }

  return ret;
}

Stmt*
Ast2Asg::operator()(ast::ExpressionStatementContext* ctx)
{
  if (auto p = ctx->expression()) {
    auto ret = make<ExprStmt>();
    ret->expr = self(p);
    return ret;
  }

  return make<NullStmt>();
}

Stmt*
Ast2Asg::operator()(ast::SelectionStatementContext* ctx)
{
  if (ctx->If()) {
    auto ret = make<IfStmt>();
    ret->cond = self(ctx->cond);

    // 保存当前循环上下文
    auto savedLoop = mCurrentLoop;

    ret->then = self(ctx->thenStmt);

    if (ctx->Else()) {
      ret->else_ = self(ctx->elseStmt);
    }

    // 恢复循环上下文
    mCurrentLoop = savedLoop;

    return ret;
  }

  ABORT();
}

Stmt*
Ast2Asg::operator()(ast::IterationStatementContext* ctx)
{
  if (ctx->While()) {
    auto ret = make<WhileStmt>();
    ret->cond = self(ctx->cond);

    // 设置当前循环
    auto savedLoop = mCurrentLoop;
    mCurrentLoop = ret;

    ret->body = self(ctx->body);

    // 恢复之前的循环
    mCurrentLoop = savedLoop;

    return ret;
  }

  if (ctx->Do()) {
    auto ret = make<DoStmt>();

    // 设置当前循环
    auto savedLoop = mCurrentLoop;
    mCurrentLoop = ret;

    ret->body = self(ctx->body);

    // 恢复之前的循环
    mCurrentLoop = savedLoop;

    ret->cond = self(ctx->cond);

    return ret;
  }

  if (ctx->For()) {
    // For 循环 - 暂不支持
    ABORT();
  }

  ABORT();
}

Stmt*
Ast2Asg::operator()(ast::JumpStatementContext* ctx)
{
  if (ctx->Return()) {
    auto ret = make<ReturnStmt>();
    ret->func = mCurrentFunc;
    if (auto p = ctx->expression())
      ret->expr = self(p);
    return ret;
  }

  if (ctx->Break()) {
    auto ret = make<BreakStmt>();
    ret->loop = mCurrentLoop;
    return ret;
  }

  if (ctx->Continue()) {
    auto ret = make<ContinueStmt>();
    ret->loop = mCurrentLoop;
    return ret;
  }

  ABORT();
}

//==============================================================================
// 声明
//==============================================================================

std::vector<Decl*>
Ast2Asg::operator()(ast::DeclarationContext* ctx)
{
  std::vector<Decl*> ret;

  auto specs = self(ctx->declarationSpecifiers());

  if (auto p = ctx->initDeclaratorList()) {
    for (auto&& j : p->initDeclarator())
      ret.push_back(self(j, specs));
  }

  // 如果 initDeclaratorList 为空则这行声明语句无意义
  return ret;
}

FunctionDecl*
Ast2Asg::operator()(ast::FunctionDefinitionContext* ctx)
{
  auto ret = make<FunctionDecl>();
  mCurrentFunc = ret;

  auto type = make<Type>();
  ret->type = type;

  auto sq = self(ctx->declarationSpecifiers());
  type->spec = sq.first, type->qual = sq.second;

  auto [texp, name, params] = self(ctx->declarator(), nullptr, true);
  // declarator 已经返回了正确的 FunctionType，直接使用
  type->texp = texp;
  ret->name = std::move(name);

  std::cerr << "DEBUG: FunctionDefinition name=" << ret->name << " params=" << params.size() << std::endl;

  // 将参数声明设置到 FunctionDecl
  for (auto paramDecl : params) {
    if (auto varDecl = paramDecl->dcst<VarDecl>()) {
      std::cerr << "DEBUG: param name=" << varDecl->name << std::endl;
      ret->params.push_back(varDecl);
    }
  }

  // 函数定义在签名之后就加入符号表，以允许递归调用
  // 注意：需要在创建函数局部符号表之前，将函数添加到外层符号表
  std::cerr << "DEBUG: mSymtbl=" << mSymtbl << std::endl;
  if (mSymtbl) {
    (*mSymtbl)[ret->name] = ret;
  }

  Symtbl localDecls(self);

  // 将函数参数添加到函数体的符号表中
  for (auto paramDecl : ret->params) {
    std::cerr << "DEBUG: adding param to localDecls: " << paramDecl->name << std::endl;
    localDecls[paramDecl->name] = paramDecl;
  }

  std::cerr << "DEBUG: Processing body for " << ret->name << std::endl;
  ret->body = self(ctx->compoundStatement());
  std::cerr << "DEBUG: Body done for " << ret->name << std::endl;

  return ret;
}

Decl*
Ast2Asg::operator()(ast::InitDeclaratorContext* ctx, SpecQual sq)
{
  auto [texp, name] = self(ctx->declarator(), nullptr);
  Decl* ret;

  if (auto funcType = texp->dcst<FunctionType>()) {
    auto fdecl = make<FunctionDecl>();
    auto type = make<Type>();
    fdecl->type = type;

    type->spec = sq.first;
    type->qual = sq.second;
    type->texp = funcType;

    fdecl->name = std::move(name);
    for (auto p : funcType->params) {
      auto paramDecl = make<VarDecl>();
      paramDecl->type = p;
      fdecl->params.push_back(paramDecl);
    }

    if (ctx->initializer())
      ABORT();
    fdecl->body = nullptr;

    ret = fdecl;
  }

  else {
    auto vdecl = make<VarDecl>();
    auto type = make<Type>();
    vdecl->type = type;

    type->spec = sq.first;
    type->qual = sq.second;
    type->texp = texp;
    vdecl->name = std::move(name);

    if (auto p = ctx->initializer())
      vdecl->init = self(p);
    else
      vdecl->init = nullptr;

    ret = vdecl;
  }

  // 这个实现允许符号重复定义，新定义会取代旧定义
  (*mSymtbl)[ret->name] = ret;
  return ret;
}

std::vector<Decl*>
Ast2Asg::operator()(ast::ParameterListContext* ctx)
{
  std::vector<Decl*> ret;
  for (auto&& i : ctx->parameterDeclaration()) {
    ret.push_back(self(i));
  }
  return ret;
}

Decl*
Ast2Asg::operator()(ast::ParameterDeclarationContext* ctx)
{
  auto specs = self(ctx->declarationSpecifiers());
  auto [texp, name] = self(ctx->declarator(), nullptr);

  auto ret = make<VarDecl>();
  auto type = make<Type>();
  ret->type = type;

  type->spec = specs.first;
  type->qual = specs.second;
  type->texp = texp;
  ret->name = std::move(name);

  return ret;
}

} // namespace asg
