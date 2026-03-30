%{
#include "lex.hpp"
/* SYsU 词法分析器 Flex 规则文件 */

using namespace lex;

#define COME(id) do { \
  int _col = g.mColumn; \
  g.mColumn += yyleng; \
  g.mTokenColumn = _col; \
  return come(id, yytext, yyleng, yylineno); \
} while(0)

#define SPACE() do { \
  g.mColumn += yyleng; \
  g.mLeadingSpace = true; \
} while(0)
%}

%option 8bit warn noyywrap yylineno

%%

 /* 预处理行指令 - 必须在行首 */
^#[ \t]*[0-9]+[ \t]*\"[^"]+\".*$  { extract_preprocessed_info(yytext, yyleng); }

 /* 关键字 - 类型说明符 */
"char"       { COME(CHAR); }
"short"      { COME(SHORT); }
"int"        { COME(INT); }
"long"       { COME(LONG); }
"float"      { COME(FLOAT); }
"double"     { COME(DOUBLE); }
"void"       { COME(VOID); }
"signed"     { COME(SIGNED); }
"unsigned"   { COME(UNSIGNED); }

 /* 关键字 - 类型限定符和存储类说明符 */
"struct"     { COME(STRUCT); }
"union"      { COME(UNION); }
"enum"       { COME(ENUM); }
"typedef"    { COME(TYPEDEF); }
"extern"     { COME(EXTERN); }
"static"     { COME(STATIC); }
"auto"       { COME(AUTO); }
"register"   { COME(REGISTER); }
"const"      { COME(CONST); }
"volatile"   { COME(VOLATILE); }
"restrict"   { COME(RESTRICT); }
"inline"     { COME(INLINE); }

 /* 关键字 - 语句 */
"if"         { COME(IF); }
"else"       { COME(ELSE); }
"while"      { COME(WHILE); }
"do"         { COME(DO); }
"for"        { COME(FOR); }
"switch"     { COME(SWITCH); }
"case"       { COME(CASE); }
"default"    { COME(DEFAULT); }
"break"      { COME(BREAK); }
"continue"   { COME(CONTINUE); }
"return"     { COME(RETURN); }
"goto"       { COME(GOTO); }
"sizeof"     { COME(SIZEOF); }

 /* 运算符 - 多字符优先 */
"..."        { COME(ELLIPSIS); }
"->"         { COME(ARROW); }
"++"         { COME(PLUSPLUS); }
"--"         { COME(MINUSMINUS); }
"<<"         { COME(LESSLESS); }
">>"         { COME(GREATERGREATER); }
"<<="        { COME(LESSLESSEQUAL); }
">>="        { COME(GREATERGREATEREQUAL); }
"<="         { COME(LESSEQUAL); }
">="         { COME(GREATEREQUAL); }
"=="         { COME(EQUALEQUAL); }
"!="         { COME(EXCLAIMEQUAL); }
"&&"         { COME(AMPAMP); }
"||"         { COME(PIPEPIPE); }
"+="         { COME(PLUSEQUAL); }
"-="         { COME(MINUSEQUAL); }
"*="         { COME(STAREQUAL); }
"/="         { COME(SLASHEQUAL); }
"%="         { COME(PERCENTEQUAL); }
"&="         { COME(AMPEQUAL); }
"|="         { COME(PIPEEQUAL); }
"^="         { COME(CARETEQUAL); }

 /* 单字符运算符和标点 */
"("          { COME(L_PAREN); }
")"          { COME(R_PAREN); }
"["          { COME(L_SQUARE); }
"]"          { COME(R_SQUARE); }
"{"          { COME(L_BRACE); }
"}"          { COME(R_BRACE); }
"+"          { COME(PLUS); }
"-"          { COME(MINUS); }
"*"          { COME(STAR); }
"/"          { COME(SLASH); }
"%"          { COME(PERCENT); }
"&"          { COME(AMP); }
"|"          { COME(PIPE); }
"^"          { COME(CARET); }
"~"          { COME(TILDE); }
"!"          { COME(EXCLAIM); }
"<"          { COME(LESS); }
">"          { COME(GREATER); }
"="          { COME(EQUAL); }
"?"          { COME(QUESTION); }
":"          { COME(COLON); }
";"          { COME(SEMI); }
","          { COME(COMMA); }
"."          { COME(PERIOD); }

 /* 标识符 */
[a-zA-Z_][a-zA-Z0-9_]*         { COME(IDENTIFIER); }

 /* 字符串字面量 */
L?\"(\\.|[^"\\\n])*\" { COME(STRING_LITERAL); }

 /* 字符常量 */
L?'(\\.|[^'\\\n])+'   { COME(CHAR_CONSTANT); }

 /* 十六进制浮点数 */
0[xX][0-9a-fA-F]*\.[0-9a-fA-F]+[pP][+-]?[0-9]+[fFlL]?   { COME(CONSTANT); }
0[xX][0-9a-fA-F]+\.[0-9a-fA-F]*[pP][+-]?[0-9]+[fFlL]?   { COME(CONSTANT); }
0[xX][0-9a-fA-F]+[pP][+-]?[0-9]+[fFlL]?                 { COME(CONSTANT); }

 /* 十进制浮点数 */
[0-9]+\.[0-9]*([eE][+-]?[0-9]+)?[fFlL]?       { COME(CONSTANT); }
[0-9]*\.[0-9]+([eE][+-]?[0-9]+)?[fFlL]?       { COME(CONSTANT); }
[0-9]+[eE][+-]?[0-9]+[fFlL]?                  { COME(CONSTANT); }

 /* 十六进制整数 */
0[xX][0-9a-fA-F]+(([uU][lL]{0,2})|([lL]{0,2}[uU]?))?   { COME(CONSTANT); }

 /* 八进制整数 */
0[0-7]*(([uU][lL]{0,2})|([lL]{0,2}[uU]?))?   { COME(CONSTANT); }

 /* 十进制整数 */
[1-9][0-9]*(([uU][lL]{0,2})|([lL]{0,2}[uU]?))?   { COME(CONSTANT); }

 /* 行注释 */
"//"[^\n]*                { g.mColumn += yyleng; }

 /* 块注释 */
"/*"([^*]|\*+[^*/])*\*+"/"  { 
                              for (int i = 0; i < yyleng; i++) {
                                if (yytext[i] == '\n') {
                                  g.mLine++;
                                  g.mColumn = 1;
                                  g.mStartOfLine = true;
                                  g.mLeadingSpace = false;
                                }
                              }
                            }

 /* 空白字符 */
[ \t\v\f]+                { SPACE(); }
\n                        { g.mLine++; g.mColumn = 1; g.mStartOfLine = true; g.mLeadingSpace = false; }
\r                        { }

 /* 文件结束 */
<<EOF>>                   { g.mTokenColumn = g.mColumn; COME(YYEOF); }

%%
