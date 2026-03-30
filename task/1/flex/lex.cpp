#include "lex.hpp"
#include <iostream>
#include <regex>
#include <string>

void
print_token();

namespace lex {

// Token 名称映射表，与 Id 枚举顺序对应
static const char* kTokenNames[] = {
  // 标识符和常量
  "identifier",       // IDENTIFIER
  "numeric_constant", // CONSTANT
  "string_literal",   // STRING_LITERAL
  "char_constant",    // CHAR_CONSTANT
  
  // 关键字 - 类型说明符
  "char",             // CHAR
  "short",            // SHORT
  "int",              // INT
  "long",             // LONG
  "float",            // FLOAT
  "double",           // DOUBLE
  "void",             // VOID
  "signed",           // SIGNED
  "unsigned",         // UNSIGNED
  
  // 关键字 - 类型限定符和存储类说明符
  "struct",           // STRUCT
  "union",            // UNION
  "enum",             // ENUM
  "typedef",          // TYPEDEF
  "extern",           // EXTERN
  "static",           // STATIC
  "auto",             // AUTO
  "register",         // REGISTER
  "const",            // CONST
  "volatile",         // VOLATILE
  "restrict",         // RESTRICT
  "inline",           // INLINE
  
  // 关键字 - 语句
  "if",               // IF
  "else",             // ELSE
  "while",            // WHILE
  "do",               // DO
  "for",              // FOR
  "switch",           // SWITCH
  "case",             // CASE
  "default",          // DEFAULT
  "break",            // BREAK
  "continue",         // CONTINUE
  "return",           // RETURN
  "goto",             // GOTO
  "sizeof",           // SIZEOF
  
  // 括号和方括号
  "l_paren",          // L_PAREN
  "r_paren",          // R_PAREN
  "l_square",         // L_SQUARE
  "r_square",         // R_SQUARE
  "l_brace",          // L_BRACE
  "r_brace",          // R_BRACE
  
  // 算术运算符
  "plusplus",         // PLUSPLUS
  "minusminus",       // MINUSMINUS
  "plus",             // PLUS
  "minus",            // MINUS
  "star",             // STAR
  "slash",            // SLASH
  "percent",          // PERCENT
  
  // 位运算符
  "amp",              // AMP
  "pipe",             // PIPE
  "caret",            // CARET
  "tilde",            // TILDE
  
  // 比较运算符
  "lessless",         // LESSLESS
  "greatergreater",   // GREATERGREATER
  "less",             // LESS
  "greater",          // GREATER
  "lessequal",        // LESSEQUAL
  "greaterequal",     // GREATEREQUAL
  "equalequal",       // EQUALEQUAL
  "exclaimequal",     // EXCLAIMEQUAL
  
  // 逻辑运算符
  "ampamp",           // AMPAMP
  "pipepipe",         // PIPEPIPE
  "exclaim",          // EXCLAIM
  
  // 赋值运算符
  "equal",            // EQUAL
  "plusequal",        // PLUSEQUAL
  "minusequal",       // MINUSEQUAL
  "starequal",        // STAREQUAL
  "slashequal",       // SLASHEQUAL
  "percentequal",     // PERCENTEQUAL
  "ampequal",         // AMPEQUAL
  "pipeequal",        // PIPEEQUAL
  "caretequal",       // CARETEQUAL
  "lesslessequal",    // LESSLESSEQUAL
  "greatergreaterequal", // GREATERGREATEREQUAL
  
  // 其他运算符和标点
  "question",         // QUESTION
  "colon",            // COLON
  "semi",             // SEMI
  "comma",            // COMMA
  "period",           // PERIOD
  "arrow",            // ARROW
  "ellipsis"          // ELLIPSIS
};

const char*
id2str(Id id)
{
  if (id == Id::YYEOF) {
    return "eof";
  }
  if (id < Id::IDENTIFIER) {
    return "<UNKNOWN>";
  }
  int index = int(id) - int(Id::IDENTIFIER);
  if (index >= 0 && index < int(sizeof(kTokenNames) / sizeof(kTokenNames[0]))) {
    return kTokenNames[index];
  }
  return "<UNKNOWN>";
}

G g;

int
come(int tokenId, const char* yytext, int yyleng, int yylineno)
{
  g.mId = Id(tokenId);
  g.mText = { yytext, std::size_t(yyleng) };
  // 使用逻辑行号而不是物理行号
  // g.mLine 已经在预处理行解析时更新

  print_token();
  g.mStartOfLine = false;
  g.mLeadingSpace = false;

  return tokenId;
}

void
space(const char* yytext, int yyleng)
{
  // 更新列号
  g.mColumn += yyleng;
  // 标记有前导空格
  g.mLeadingSpace = true;
}

void
extract_preprocessed_info(const char* yytext, int yyleng)
{
  // 解析预处理行: # linenum "filename" [flags]
  std::string text(yytext, yyleng);
  
  // 使用正则表达式提取行号和文件名
  // 格式: # linenum "filename" [flags]
  static std::regex preproc_regex("^#\\s+(\\d+)\\s+\"([^\"]+)\"");
  std::smatch match;
  if (std::regex_search(text, match, preproc_regex)) {
    int lineNum = std::stoi(match[1].str());
    std::string filename = match[2].str();
    
    // 更新全局状态
    g.mFile = filename;
    // 预处理行指定的行号是下一行的逻辑行号
    // 预处理行后面的换行会使得 g.mLine++（在 \n 规则中）
    // 所以这里设置 g.mLine = lineNum - 1，换行后变成 lineNum
    g.mLine = lineNum - 1;
    g.mColumn = 1;
    g.mStartOfLine = true;
    g.mLeadingSpace = false;
  }
}

} // namespace lex
