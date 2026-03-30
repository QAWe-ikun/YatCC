#include "lex.hpp"
#include "lex.l.hh"
#include <fstream>
#include <iostream>

static std::ofstream outFile;

// 转义字符串中的特殊字符
static std::string
escape(std::string_view sv)
{
  std::string result;
  for (char c : sv) {
    switch (c) {
      case '\n':
        result += "\\n";
        break;
      case '\t':
        result += "\\t";
        break;
      case '\r':
        result += "\\r";
        break;
      case '\v':
        result += "\\v";
        break;
      case '\f':
        result += "\\f";
        break;
      case '\a':
        result += "\\a";
        break;
      case '\b':
        result += "\\b";
        break;
      case '\\':
        result += "\\\\";
        break;
      case '\'':
        result += "\\\'";
        break;
      case '\0':
        break;
      default:
        result += c;
        break;
    }
  }
  return result;
}

// 打印 token 到输出文件
void
print_token()
{
  // 输出 token 类型和文本
  if (lex::g.mId == lex::Id::YYEOF) {
    outFile << lex::id2str(lex::g.mId) << " ''";
  } else {
    outFile << lex::id2str(lex::g.mId) << " '" << escape(lex::g.mText) << "'";
  }
  
  // 输出标志
  if (lex::g.mId != lex::Id::YYEOF) {
    if (lex::g.mStartOfLine) {
      outFile << "\t[StartOfLine]";
    }
    if (lex::g.mLeadingSpace) {
      outFile << " [LeadingSpace]";
    }
    if (!lex::g.mStartOfLine && !lex::g.mLeadingSpace) {
      outFile << "\t";
    }
  } else {
    outFile << "\t";
  }
  
  // 输出位置信息 - 使用 mTokenColumn 而不是 mColumn
  outFile << " Loc=<" << lex::g.mFile << ":" << lex::g.mLine << ":" << lex::g.mTokenColumn << ">\n";
  outFile << std::flush;
}

int
main(int argc, char* argv[])
{
  if (argc != 3) {
    std::cout << "Usage: " << argv[0] << " <input> <output>\n";
    return -1;
  }

  yyin = fopen(argv[1], "r");
  if (!yyin) {
    std::cerr << "Failed to open " << argv[1] << '\n';
    return -2;
  }

  outFile = std::ofstream(argv[2]);
  if (!outFile) {
    std::cerr << "Failed to open " << argv[2] << '\n';
    return -3;
  }

  std::cout << "程序 '" << argv[0] << std::endl;
  std::cout << "输入 '" << argv[1] << std::endl;
  std::cout << "输出 '" << argv[2] << std::endl;

  // 初始化全局状态
  lex::g.mFile = argv[1];
  lex::g.mLine = 1;
  lex::g.mColumn = 1;
  lex::g.mStartOfLine = true;
  lex::g.mLeadingSpace = false;

  // 这个循环完成词法分析，yylex()中会调用print_token()，从而向
  // 输出文件中写入词法分析结果。
  while (yylex())
    ;

  fclose(yyin);
  return 0;
}
