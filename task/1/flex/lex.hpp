#pragma once

#include <string>
#include <string_view>
#include <cstring>

namespace lex {

// Token ID 枚举，完整覆盖 SYsU 语言的所有 token 类型
enum Id
{
  YYEMPTY = -2,
  YYEOF = 0,     /* "end of file"  */
  YYerror = 256, /* error  */
  YYUNDEF = 257, /* "invalid token"  */
  
  // 标识符和常量
  IDENTIFIER,
  CONSTANT,
  STRING_LITERAL,
  CHAR_CONSTANT,
  
  // 关键字 - 类型说明符
  CHAR,
  SHORT,
  INT,
  LONG,
  FLOAT,
  DOUBLE,
  VOID,
  SIGNED,
  UNSIGNED,
  
  // 关键字 - 类型限定符和存储类说明符
  STRUCT,
  UNION,
  ENUM,
  TYPEDEF,
  EXTERN,
  STATIC,
  AUTO,
  REGISTER,
  CONST,
  VOLATILE,
  RESTRICT,
  INLINE,
  
  // 关键字 - 语句
  IF,
  ELSE,
  WHILE,
  DO,
  FOR,
  SWITCH,
  CASE,
  DEFAULT,
  BREAK,
  CONTINUE,
  RETURN,
  GOTO,
  SIZEOF,
  
  // 括号和方括号
  L_PAREN,
  R_PAREN,
  L_SQUARE,
  R_SQUARE,
  L_BRACE,
  R_BRACE,
  
  // 算术运算符
  PLUSPLUS,
  MINUSMINUS,
  PLUS,
  MINUS,
  STAR,
  SLASH,
  PERCENT,
  
  // 位运算符
  AMP,
  PIPE,
  CARET,
  TILDE,
  
  // 比较运算符
  LESSLESS,
  GREATERGREATER,
  LESS,
  GREATER,
  LESSEQUAL,
  GREATEREQUAL,
  EQUALEQUAL,
  EXCLAIMEQUAL,
  
  // 逻辑运算符
  AMPAMP,
  PIPEPIPE,
  EXCLAIM,
  
  // 赋值运算符
  EQUAL,
  PLUSEQUAL,
  MINUSEQUAL,
  STAREQUAL,
  SLASHEQUAL,
  PERCENTEQUAL,
  AMPEQUAL,
  PIPEEQUAL,
  CARETEQUAL,
  LESSLESSEQUAL,
  GREATERGREATEREQUAL,
  
  // 其他运算符和标点
  QUESTION,
  COLON,
  SEMI,
  COMMA,
  PERIOD,
  ARROW,
  ELLIPSIS
};

// Token ID 到字符串的映射
const char*
id2str(Id id);

// 全局状态结构体
struct G
{
  Id mId{ YYEOF };              // 词号
  std::string_view mText;       // 对应文本
  std::string mFile;            // 文件路径
  int mLine{ 1 };               // 逻辑行号
  int mColumn{ 1 };             // 当前列号（下一个 token 的起始列）
  int mTokenColumn{ 1 };        // 当前 token 的起始列号
  bool mStartOfLine{ true };    // 是否是行首
  bool mLeadingSpace{ false };  // 是否有前导空格
};

extern G g;

// 处理 token 并返回 ID
int
come(int tokenId, const char* yytext, int yyleng, int yylineno);

// 从预处理行中提取信息
void
extract_preprocessed_info(const char* yytext, int yyleng);

} // namespace lex
