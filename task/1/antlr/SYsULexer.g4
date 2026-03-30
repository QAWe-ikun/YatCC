lexer grammar SYsULexer;

channels { PREPROC }

// ==================== 关键字 (35个) ====================
// 类型说明符
Char : 'char';
Short : 'short';
Int : 'int';
Long : 'long';
Float : 'float';
Double : 'double';
Void : 'void';
Signed : 'signed';
Unsigned : 'unsigned';

// 类型限定符和存储类说明符
Struct : 'struct';
Union : 'union';
Enum : 'enum';
Typedef : 'typedef';
Extern : 'extern';
Static : 'static';
Auto : 'auto';
Register : 'register';
Const : 'const';
Volatile : 'volatile';
Restrict : 'restrict';
Inline : 'inline';

// 语句关键字
If : 'if';
Else : 'else';
While : 'while';
Do : 'do';
For : 'for';
Switch : 'switch';
Case : 'case';
Default : 'default';
Break : 'break';
Continue : 'continue';
Return : 'return';
Goto : 'goto';
Sizeof : 'sizeof';

// ==================== 运算符和标点符号 ====================
// 括号和方括号
LeftParen : '(';
RightParen : ')';
LeftBracket : '[';
RightBracket : ']';
LeftBrace : '{';
RightBrace : '}';

// 算术运算符
PlusPlus : '++';
MinusMinus : '--';
Plus : '+';
Minus : '-';
Star : '*';
Slash : '/';
Percent : '%';

// 位运算符
Amp : '&';
Pipe : '|';
Caret : '^';
Tilde : '~';

// 比较运算符
LessLess : '<<';
GreaterGreater : '>>';
Less : '<';
Greater : '>';
LessEqual : '<=';
GreaterEqual : '>=';
EqualEqual : '==';
ExclaimEqual : '!=';

// 逻辑运算符
AmpAmp : '&&';
PipePipe : '||';
Exclaim : '!';

// 赋值运算符
Equal : '=';
PlusEqual : '+=';
MinusEqual : '-=';
StarEqual : '*=';
SlashEqual : '/=';
PercentEqual : '%=';
AmpEqual : '&=';
PipeEqual : '|=';
CaretEqual : '^=';
LessLessEqual : '<<=';
GreaterGreaterEqual : '>>=';

// 其他运算符和标点
Question : '?';
Colon : ':';
Semi : ';';
Comma : ',';
Period : '.';
Arrow : '->';
Ellipsis : '...';

// ==================== 标识符 ====================
Identifier
    :   IdentifierNondigit
        (   IdentifierNondigit
        |   Digit
        )*
    ;

fragment
IdentifierNondigit
    :   Nondigit
    ;

fragment
Nondigit
    :   [a-zA-Z_]
    ;

fragment
Digit
    :   [0-9]
    ;

// ==================== 数值常量 ====================
Constant
    :   IntegerConstant
    |   FloatingConstant
    ;

// 整数常量
fragment
IntegerConstant
    :   DecimalConstant IntegerSuffix?
    |   OctalConstant IntegerSuffix?
    |   HexadecimalConstant IntegerSuffix?
    ;

fragment
DecimalConstant
    :   NonzeroDigit Digit*
    ;

fragment
OctalConstant
    :   '0' OctalDigit*
    ;

fragment
HexadecimalConstant
    :   HexadecimalPrefix HexadecimalDigit+
    ;

fragment
HexadecimalPrefix
    :   '0' [xX]
    ;

fragment
NonzeroDigit
    :   [1-9]
    ;

fragment
OctalDigit
    :   [0-7]
    ;

fragment
HexadecimalDigit
    :   [0-9a-fA-F]
    ;

fragment
IntegerSuffix
    :   [uU] ([lL] | 'll' | 'LL')?
    |   ([lL] | 'll' | 'LL') [uU]?
    ;

// 浮点常量
fragment
FloatingConstant
    :   DecimalFloatingConstant
    |   HexadecimalFloatingConstant
    ;

fragment
DecimalFloatingConstant
    :   FractionalConstant ExponentPart? FloatingSuffix?
    |   Digit+ ExponentPart FloatingSuffix?
    ;

fragment
FractionalConstant
    :   Digit* '.' Digit+
    |   Digit+ '.'
    ;

fragment
ExponentPart
    :   [eE] [+-]? Digit*
    ;

fragment
HexadecimalFloatingConstant
    :   HexadecimalPrefix HexadecimalFractionalConstant BinaryExponentPart FloatingSuffix?
    |   HexadecimalPrefix HexadecimalDigit+ BinaryExponentPart FloatingSuffix?
    ;

fragment
HexadecimalFractionalConstant
    :   HexadecimalDigit* '.' HexadecimalDigit+
    |   HexadecimalDigit+ '.'
    ;

fragment
BinaryExponentPart
    :   [pP] [+-]? Digit+
    ;

fragment
FloatingSuffix
    :   [fFlL]
    ;

// ==================== 字符常量 ====================
CharacterConstant
    :   'L'? '\'' CCharSequence '\''
    ;

fragment
CCharSequence
    :   CChar+
    ;

fragment
CChar
    :   ~['\\\r\n]
    |   EscapeSequence
    ;

// ==================== 字符串字面量 ====================
StringLiteral
    :   'L'? '"' SCharSequence? '"'
    ;

fragment
SCharSequence
    :   SChar+
    ;

fragment
SChar
    :   ~["\\\r\n]
    |   EscapeSequence
    ;

fragment
EscapeSequence
    :   '\\' ['"?abfnrtv\\]
    |   '\\' OctalDigit OctalDigit? OctalDigit?
    |   '\\x' HexadecimalDigit+
    |   UniversalCharacterName
    ;

fragment
UniversalCharacterName
    :   '\\u' HexadecimalDigit HexadecimalDigit HexadecimalDigit HexadecimalDigit
    |   '\\U' HexadecimalDigit HexadecimalDigit HexadecimalDigit HexadecimalDigit
            HexadecimalDigit HexadecimalDigit HexadecimalDigit HexadecimalDigit
    ;

// ==================== 预处理行指令 ====================
// 格式: # linenum "filename" [flags]
LineAfterPreprocessing
    :   '#' Whitespace? Digit+ Whitespace? StringLiteral (Whitespace Digit+)? (Whitespace ~[\r\n]*)?
        -> skip
    ;

// ==================== 空白和注释 ====================
Whitespace
    :   [ \t]+
        -> skip
    ;

Newline
    :   (   '\r' '\n'?
        |   '\n'
        )
        -> skip
    ;

BlockComment
    :   '/*' .*? '*/'
        -> skip
    ;

LineComment
    :   '//' ~[\r\n]*
        -> skip
    ;