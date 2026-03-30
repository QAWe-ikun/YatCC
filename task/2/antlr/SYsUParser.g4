parser grammar SYsUParser;

options {
  tokenVocab=SYsULexer;
}

//==============================================================================
// 表达式
//==============================================================================

primaryExpression
    :   Identifier
    |   Constant
    |   LeftParen expression RightParen
    ;

postfixExpression
    :   primaryExpression
    |   postfixExpression LeftBracket expression RightBracket     // 数组下标
    |   postfixExpression LeftParen argumentExpressionList? RightParen  // 函数调用
    |   postfixExpression Dot Identifier                          // 结构体成员访问
    |   postfixExpression Arrow Identifier                        // 结构体指针成员访问
    |   postfixExpression PlusPlus                                // 后置++
    |   postfixExpression MinusMinus                              // 后置--
    ;

argumentExpressionList
    :   assignmentExpression (Comma assignmentExpression)*
    ;

unaryExpression
    :   postfixExpression
    |   PlusPlus unaryExpression                                  // 前置++
    |   MinusMinus unaryExpression                                // 前置--
    |   unaryOperator unaryExpression
    ;

unaryOperator
    :   Plus | Minus | Exclaim | Tilde
    ;

multiplicativeExpression
    :   unaryExpression ((Star | Slash | Percent) unaryExpression)*
    ;

additiveExpression
    :   multiplicativeExpression ((Plus | Minus) multiplicativeExpression)*
    ;

shiftExpression
    :   additiveExpression ((LessLess | GreaterGreater) additiveExpression)*
    ;

relationalExpression
    :   shiftExpression ((Less | Greater | LessEqual | GreaterEqual) shiftExpression)*
    ;

equalityExpression
    :   relationalExpression ((EqualEqual | ExclaimEqual) relationalExpression)*
    ;

andExpression
    :   equalityExpression (Amp equalityExpression)*
    ;

exclusiveOrExpression
    :   andExpression (Caret andExpression)*
    ;

inclusiveOrExpression
    :   exclusiveOrExpression (Pipe exclusiveOrExpression)*
    ;

logicalAndExpression
    :   inclusiveOrExpression (AmpAmp inclusiveOrExpression)*
    ;

logicalOrExpression
    :   logicalAndExpression (PipePipe logicalAndExpression)*
    ;

conditionalExpression
    :   logicalOrExpression (Question expression Colon conditionalExpression)?
    ;

assignmentExpression
    :   conditionalExpression
    |   unaryExpression assignmentOperator assignmentExpression
    ;

assignmentOperator
    :   Equal
    |   PlusEqual
    |   MinusEqual
    |   StarEqual
    |   SlashEqual
    |   PercentEqual
    |   AmpEqual
    |   PipeEqual
    |   CaretEqual
    |   LessLessEqual
    |   GreaterGreaterEqual
    ;

expression
    :   assignmentExpression (Comma assignmentExpression)*
    ;

//==============================================================================
// 声明
//==============================================================================

declaration
    :   declarationSpecifiers initDeclaratorList? Semi
    ;

declarationSpecifiers
    :   declarationSpecifier+
    ;

declarationSpecifier
    :   typeSpecifier
    |   typeQualifier
    ;

initDeclaratorList
    :   initDeclarator (Comma initDeclarator)*
    ;

initDeclarator
    :   declarator (Equal initializer)?
    ;

typeSpecifier
    :   Void
    |   Char
    |   Int
    |   Long
    |   Long Long    // long long
    ;

typeQualifier
    :   Const
    ;

declarator
    :   directDeclarator
    ;

directDeclarator
    :   Identifier
    |   directDeclarator LeftBracket assignmentExpression? RightBracket  // 数组声明
    |   directDeclarator LeftParen parameterTypeList? RightParen         // 函数声明
    ;

parameterTypeList
    :   parameterList
    ;

parameterList
    :   parameterDeclaration (Comma parameterDeclaration)*
    ;

parameterDeclaration
    :   declarationSpecifiers declarator
    ;

identifierList
    :   Identifier (Comma Identifier)*
    ;

initializer
    :   assignmentExpression
    |   LeftBrace (initializerList Comma?)? RightBrace
    ;

initializerList
    :   initializer (Comma initializer)*
    ;

//==============================================================================
// 语句
//==============================================================================

statement
    :   compoundStatement
    |   expressionStatement
    |   selectionStatement
    |   iterationStatement
    |   jumpStatement
    ;

compoundStatement
    :   LeftBrace blockItemList? RightBrace
    ;

blockItemList
    :   blockItem+
    ;

blockItem
    :   statement
    |   declaration
    ;

expressionStatement
    :   expression? Semi
    ;

selectionStatement
    :   If LeftParen cond=expression RightParen thenStmt=statement (Else elseStmt=statement)?
    ;

iterationStatement
    :   While LeftParen cond=expression RightParen body=statement
    |   Do body=statement While LeftParen cond=expression RightParen Semi
    |   For LeftParen init=expression? Semi cond=expression? Semi step=expression? RightParen body=statement
    ;

jumpStatement
    :   Return expression? Semi
    |   Break Semi
    |   Continue Semi
    ;

//==============================================================================
// 顶层
//==============================================================================

compilationUnit
    :   translationUnit? EOF
    ;

translationUnit
    :   externalDeclaration+
    ;

externalDeclaration
    :   functionDefinition
    |   declaration
    ;

functionDefinition
    :   declarationSpecifiers declarator compoundStatement
    ;
