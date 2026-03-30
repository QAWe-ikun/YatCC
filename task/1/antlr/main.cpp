#include "SYsULexer.h"
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <sstream>
#include <vector>
#include <regex>
#include <string>

// 预处理指令信息结构体
struct PreprocessorDirective {
    int source_line;      // 预处理指令在源文件中的物理行号
    int target_line;      // 预处理指令指定的逻辑行号
    std::string filename; // 预处理指令指定的文件名
    int flag;             // 预处理指令的标志位（如果有）
};

// ANTLR token 类型名到 clang token 类型名的映射
// 必须与 SYsULexer.g4 中定义的 token 名称完全对应
std::unordered_map<std::string, std::string> tokenTypeMapping = {
    // 关键字 - 类型说明符
    { "Char", "char" },
    { "Short", "short" },
    { "Int", "int" },
    { "Long", "long" },
    { "Float", "float" },
    { "Double", "double" },
    { "Void", "void" },
    { "Signed", "signed" },
    { "Unsigned", "unsigned" },
    
    // 关键字 - 类型限定符和存储类说明符
    { "Struct", "struct" },
    { "Union", "union" },
    { "Enum", "enum" },
    { "Typedef", "typedef" },
    { "Extern", "extern" },
    { "Static", "static" },
    { "Auto", "auto" },
    { "Register", "register" },
    { "Const", "const" },
    { "Volatile", "volatile" },
    { "Restrict", "restrict" },
    { "Inline", "inline" },
    
    // 关键字 - 语句
    { "If", "if" },
    { "Else", "else" },
    { "While", "while" },
    { "Do", "do" },
    { "For", "for" },
    { "Switch", "switch" },
    { "Case", "case" },
    { "Default", "default" },
    { "Break", "break" },
    { "Continue", "continue" },
    { "Return", "return" },
    { "Goto", "goto" },
    { "Sizeof", "sizeof" },
    
    // 括号和方括号
    { "LeftParen", "l_paren" },
    { "RightParen", "r_paren" },
    { "LeftBracket", "l_square" },
    { "RightBracket", "r_square" },
    { "LeftBrace", "l_brace" },
    { "RightBrace", "r_brace" },
    
    // 算术运算符
    { "PlusPlus", "plusplus" },
    { "MinusMinus", "minusminus" },
    { "Plus", "plus" },
    { "Minus", "minus" },
    { "Star", "star" },
    { "Slash", "slash" },
    { "Percent", "percent" },
    
    // 位运算符
    { "Amp", "amp" },
    { "Pipe", "pipe" },
    { "Caret", "caret" },
    { "Tilde", "tilde" },
    
    // 比较运算符
    { "LessLess", "lessless" },
    { "GreaterGreater", "greatergreater" },
    { "Less", "less" },
    { "Greater", "greater" },
    { "LessEqual", "lessequal" },
    { "GreaterEqual", "greaterequal" },
    { "EqualEqual", "equalequal" },
    { "ExclaimEqual", "exclaimequal" },
    
    // 逻辑运算符
    { "AmpAmp", "ampamp" },
    { "PipePipe", "pipepipe" },
    { "Exclaim", "exclaim" },
    
    // 赋值运算符
    { "Equal", "equal" },
    { "PlusEqual", "plusequal" },
    { "MinusEqual", "minusequal" },
    { "StarEqual", "starequal" },
    { "SlashEqual", "slashequal" },
    { "PercentEqual", "percentequal" },
    { "AmpEqual", "ampequal" },
    { "PipeEqual", "pipeequal" },
    { "CaretEqual", "caretequal" },
    { "LessLessEqual", "lesslessequal" },
    { "GreaterGreaterEqual", "greatergreaterequal" },
    
    // 其他运算符和标点
    { "Question", "question" },
    { "Colon", "colon" },
    { "Semi", "semi" },
    { "Comma", "comma" },
    { "Period", "period" },
    { "Arrow", "arrow" },
    { "Ellipsis", "ellipsis" },
    
    // 标识符和常量
    { "Identifier", "identifier" },
    { "Constant", "numeric_constant" },
    { "CharacterConstant", "char_constant" },
    { "StringLiteral", "string_literal" },
    
    // EOF
    { "EOF", "eof" },
};

// 解析源文件中的预处理指令
// 格式: # linenum "filename" [flags]
std::vector<PreprocessorDirective> parsePreprocessorDirectives(const std::string& sourceCode) {
    std::vector<PreprocessorDirective> directives;
    std::istringstream stream(sourceCode);
    std::string line;
    int current_line = 1;
    
    // 正则表达式匹配预处理指令格式
    // 捕获组: 1=行号, 2=文件名, 3=标志位(可选)
    std::regex directive_regex("^\\s*#\\s+(\\d+)\\s+\"([^\"]+)\"(?:\\s+(\\d+))?.*$");
    
    while (std::getline(stream, line)) {
        std::smatch match;
        if (std::regex_match(line, match, directive_regex)) {
            PreprocessorDirective dir;
            dir.source_line = current_line;
            dir.target_line = std::stoi(match[1].str());
            dir.filename = match[2].str();
            if (match[3].matched) {
                dir.flag = std::stoi(match[3].str());
            } else {
                dir.flag = 0;
            }
            directives.push_back(dir);
        }
        current_line++;
    }
    
    return directives;
}

// 根据预处理指令调整 token 的位置信息
void adjustLocation(const std::vector<PreprocessorDirective>& directives,
                   int token_line,
                   std::string& out_filename,
                   int& out_line) {
    if (directives.empty()) {
        out_line = token_line;
        return;
    }

    // 找到最后一个 source_line < token_line 的预处理指令
    int best_source_line = 0;
    int best_target_line = 1;
    std::string best_filename = directives[0].filename;

    for (const auto& dir : directives) {
        if (dir.source_line < token_line) {
            best_source_line = dir.source_line;
            best_target_line = dir.target_line;
            best_filename = dir.filename;
        }
    }

    // 计算逻辑行号
    // 预处理指令 # N "filename" 表示从下一行开始，代码来自 filename 的第 N 行
    // 所以逻辑行号 = target_line + (token_line - source_line - 1)
    // 但根据规格，提取的行号需要减 1，因为紧跟的换行会将行号 +1
    int line_offset = token_line - best_source_line - 1;
    out_filename = best_filename;
    // target_line 已经是正确的起始行号，不需要再减 1
    out_line = best_target_line + line_offset;
}

// 获取初始文件名
std::string getInitialFileName(const std::vector<PreprocessorDirective>& directives) {
    if (!directives.empty()) {
        return directives[0].filename;
    }
    return "<unknown>";
}

// 获取初始行号
int getInitialLine(const std::vector<PreprocessorDirective>& directives) {
    if (!directives.empty()) {
        return directives[0].target_line;
    }
    return 1;
}

// 打印单个 token
void print_token(const antlr4::Token* token,
                std::ofstream& outFile,
                const antlr4::Lexer& lexer,
                const std::vector<PreprocessorDirective>& directives,
                const std::string& defaultFileName,
                std::set<int>& processedLines,
                int& previousEndColumn,
                int& logicalLine,
                std::string& currentFileName)
{
    auto& vocabulary = lexer.getVocabulary();
    
    // 获取 token 类型名
    std::string tokenTypeName = std::string(vocabulary.getSymbolicName(token->getType()));
    if (tokenTypeName.empty()) {
        tokenTypeName = "<UNKNOWN>";
    }
    
    // 映射到 clang 格式的 token 类型名
    if (tokenTypeMapping.find(tokenTypeName) != tokenTypeMapping.end()) {
        tokenTypeName = tokenTypeMapping[tokenTypeName];
    }
    
    // 获取 token 的物理位置
    int physicalLine = token->getLine();
    int column = token->getCharPositionInLine() + 1; // 转换为 1-based
    
    // 判断是否是行的第一个 token
    bool isFirstTokenInPhysicalLine = (processedLines.find(physicalLine) == processedLines.end());
    if (isFirstTokenInPhysicalLine) {
        processedLines.insert(physicalLine);
        previousEndColumn = 0;
    }
    
    // StartOfLine: 仅当该 token 是逻辑行的第一个可见 token 时为 true
    bool startOfLine = isFirstTokenInPhysicalLine;
    
    // LeadingSpace: 当该 token 前有空白字符时为 true
    // 对于行首 token，如果 column > 1，说明前面有缩进空格
    // 对于非行首 token，如果 column > previousEndColumn + 1，说明前面有空白
    bool leadingSpace = false;
    if (isFirstTokenInPhysicalLine) {
        // 行首 token，检查是否有缩进
        if (column > 1) {
            leadingSpace = true;
        }
    } else {
        // 非行首 token，检查前面是否有空白
        if (column > previousEndColumn + 1) {
            leadingSpace = true;
        }
    }
    
    // 更新前一个 token 的结束列号
    std::string tokenText = token->getText();
    if (tokenText != "<EOF>") {
        previousEndColumn = column + static_cast<int>(tokenText.length()) - 1;
    }
    
    // 根据预处理指令调整位置信息
    std::string fileName = currentFileName;
    int adjustedLine = logicalLine;
    adjustLocation(directives, physicalLine, fileName, adjustedLine);
    currentFileName = fileName;
    
    // 更新逻辑行号（用于下一个 token）
    logicalLine = adjustedLine;
    
    // 输出 token
    if (tokenText == "<EOF>") {
        outFile << tokenTypeName << " ''";
    } else {
        outFile << tokenTypeName << " '" << tokenText << "'";
    }
    
    // 输出标志
    if (tokenText != "<EOF>") {
        if (startOfLine) {
            outFile << "\t [StartOfLine]";
        }
        if (leadingSpace) {
            outFile << " [LeadingSpace]";
        }
        if (!startOfLine && !leadingSpace) {
            // 若都无，输出一个 \t 占位
            outFile << "\t";
        }
    } else {
        // EOF 也需要格式化
        outFile << "\t";
    }
    
    // 输出位置信息
    if (tokenText == "<EOF>") {
        // EOF 的位置是最后一个字符之后
        outFile << " Loc=<" << fileName << ":" << adjustedLine << ":" << (column + 1) << ">" << std::endl;
    } else {
        outFile << " Loc=<" << fileName << ":" << adjustedLine << ":" << column << ">" << std::endl;
    }
}

int main(int argc, char* argv[])
{
    if (argc != 3) {
        std::cout << "Usage: " << argv[0] << " <input> <output>\n";
        return -1;
    }

    std::ifstream inFile(argv[1]);
    if (!inFile) {
        std::cout << "Error: unable to open input file: " << argv[1] << '\n';
        return -2;
    }

    std::ofstream outFile(argv[2]);
    if (!outFile) {
        std::cout << "Error: unable to open output file: " << argv[2] << '\n';
        return -3;
    }

    // 读取整个文件内容
    inFile.seekg(0, std::ios::end);
    size_t length = inFile.tellg();
    inFile.seekg(0, std::ios::beg);
    
    std::string sourceCode(length, '\0');
    inFile.read(&sourceCode[0], length);
    inFile.close();

    // 解析预处理指令
    std::vector<PreprocessorDirective> directives = parsePreprocessorDirectives(sourceCode);
    
    // 获取初始文件名
    std::string fileName = getInitialFileName(directives);

    // 创建 ANTLR 输入流和 lexer
    antlr4::ANTLRInputStream input(sourceCode);
    SYsULexer lexer(&input);

    // 获取所有 token
    std::vector<std::unique_ptr<antlr4::Token>> allTokens;
    std::unique_ptr<antlr4::Token> token;
    while ((token = lexer.nextToken()) != nullptr) {
        if (token->getType() == antlr4::Token::EOF) {
            allTokens.push_back(std::move(token));
            break;
        }
        allTokens.push_back(std::move(token));
    }

    // 处理并输出每个 token
    std::set<int> processedLines;
    int previousEndColumn = 0;
    int logicalLine = getInitialLine(directives);
    std::string currentFileName = fileName;
    
    // 重置 lexer 以便重新获取 token
    lexer.reset();
    
    for (auto& t : allTokens) {
        print_token(t.get(), outFile, lexer, directives, fileName, processedLines, 
                   previousEndColumn, logicalLine, currentFileName);
    }

    return 0;
}