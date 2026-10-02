%top{

#include "support/type/CompilationStatus.h"
#include "support/type/FlexContext.h"
#include "support/type/TokenLabel.h"
#include "frontend/syntactic-analysis/AbstractSyntaxTree.h"
#include "frontend/syntactic-analysis/BisonParser.h"

}

%{

extern CompilationStatus TokenLexemeAction(TokenLabel label);
extern CompilationStatus IntegerLexemeAction(void);
extern CompilationStatus IdentifierLexemeAction(void);
extern CompilationStatus EnterMultilineCommentLexemeAction(FlexContext context);
extern CompilationStatus LeaveMultilineCommentLexemeAction(void);
extern CompilationStatus IgnoredLexemeAction(void);
extern CompilationStatus EOFLexemeAction(void);
extern CompilationStatus UnknownLexemeAction(void);

%}

%x MULTILINE_COMMENT

whitespace      [[:space:]]+
digit           [0-9]
letter          [a-zA-Z_]
alnum           [a-zA-Z_0-9]

%%

%{
%}

"/*"                        { return EnterMultilineCommentLexemeAction(MULTILINE_COMMENT); }
<MULTILINE_COMMENT>"*/"     { return LeaveMultilineCommentLexemeAction(); }
<MULTILINE_COMMENT>[^*]+    { return IgnoredLexemeAction(); }
<MULTILINE_COMMENT>.        { return IgnoredLexemeAction(); }

"//"[^\n]*                  { return IgnoredLexemeAction(); }

"int8_t"        { return TokenLexemeAction(INT8_T); }
"int16_t"       { return TokenLexemeAction(INT16_T); }
"int32_t"       { return TokenLexemeAction(INT32_T); }
"int64_t"       { return TokenLexemeAction(INT64_T); }
"uint8_t"       { return TokenLexemeAction(UINT8_T); }
"uint16_t"      { return TokenLexemeAction(UINT16_T); }
"uint32_t"      { return TokenLexemeAction(UINT32_T); }
"uint64_t"      { return TokenLexemeAction(UINT64_T); }
"bool"          { return TokenLexemeAction(BOOL_KW); }
"void"          { return TokenLexemeAction(VOID_KW); }

"constexpr"     { return TokenLexemeAction(KW_CONSTEXPR); }
"if"            { return TokenLexemeAction(KW_IF); }
"else"          { return TokenLexemeAction(KW_ELSE); }
"for"           { return TokenLexemeAction(KW_FOR); }
"while"         { return TokenLexemeAction(KW_WHILE); }
"return"        { return TokenLexemeAction(KW_RETURN); }
"unique"        { return TokenLexemeAction(KW_UNIQUE); }
"aliased"       { return TokenLexemeAction(KW_ALIASED); }
"__parallel"    { return TokenLexemeAction(KW_PARALLEL); }
"__abs"         { return TokenLexemeAction(KW_ABS); }
"__min"         { return TokenLexemeAction(KW_MIN); }
"__max"         { return TokenLexemeAction(KW_MAX); }
"true"          { return TokenLexemeAction(KW_TRUE); }
"false"         { return TokenLexemeAction(KW_FALSE); }

"+="            { return TokenLexemeAction(ADD_ASSIGN); }
"-="            { return TokenLexemeAction(SUB_ASSIGN); }
"*="            { return TokenLexemeAction(MUL_ASSIGN); }
"=="            { return TokenLexemeAction(EQ); }
"!="            { return TokenLexemeAction(NEQ); }
"<="            { return TokenLexemeAction(LE); }
">="            { return TokenLexemeAction(GE); }
"&&"            { return TokenLexemeAction(AND); }
"||"            { return TokenLexemeAction(OR); }
"++"            { return TokenLexemeAction(INC); }
"--"            { return TokenLexemeAction(DEC); }
"<<"            { return TokenLexemeAction(LSHIFT); }
">>"            { return TokenLexemeAction(RSHIFT); }

"+"             { return TokenLexemeAction(ADD); }
"-"             { return TokenLexemeAction(SUB); }
"*"             { return TokenLexemeAction(MUL); }
"="             { return TokenLexemeAction(ASSIGN); }
"<"             { return TokenLexemeAction(LT); }
">"             { return TokenLexemeAction(GT); }
"!"             { return TokenLexemeAction(NOT); }
"&"             { return TokenLexemeAction(REF); }

"("             { return TokenLexemeAction(OPEN_PAREN); }
")"             { return TokenLexemeAction(CLOSE_PAREN); }
"{"             { return TokenLexemeAction(OPEN_BRACE); }
"}"             { return TokenLexemeAction(CLOSE_BRACE); }
"["             { return TokenLexemeAction(OPEN_BRACKET); }
"]"             { return TokenLexemeAction(CLOSE_BRACKET); }
";"             { return TokenLexemeAction(SEMICOLON); }
","             { return TokenLexemeAction(COMMA); }

{digit}+        { return IntegerLexemeAction(); }

{letter}{alnum}* { return IdentifierLexemeAction(); }

{whitespace}    { return IgnoredLexemeAction(); }
.               { return UnknownLexemeAction(); }

<<EOF>>         { return EOFLexemeAction(); }

%%

#include "FlexActions.h"
#include "FlexExport.h"
