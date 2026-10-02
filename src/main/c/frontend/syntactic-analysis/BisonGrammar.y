%code requires {
#include "support/type/TokenLabel.h"
#include "AbstractSyntaxTree.h"
}

%{

#include "BisonActions.h"

void yyerror(const YYLTYPE *location, const char *message) {}

%}

%define api.pure full
%define api.push-pull push
%define api.value.union.name SemanticValue
%define parse.error detailed
%locations
%expect 1

%union {
    signed int     integer;
    char          *strValue;
    TokenLabel     token;

    TypeKind       typeKind;
    AliasQualifier aliasQualifier;

    Expr          *expr;
    ExprList      *exprList;
    Stmt          *stmt;
    StmtList      *stmtList;
    Param         *param;
    ParamList     *paramList;
    Decl          *decl;
    DeclList      *declList;
    Program       *program;
}

%destructor { free($$); }             <strValue>
%destructor { destroyExpr($$); }      <expr>
%destructor { destroyExprList($$); }  <exprList>
%destructor { destroyStmt($$); }      <stmt>
%destructor { destroyStmtList($$); }  <stmtList>
%destructor { destroyParam($$); }     <param>
%destructor { destroyParamList($$); } <paramList>
%destructor { destroyDecl($$); }      <decl>
%destructor { destroyDeclList($$); }  <declList>

%token <integer>   INTEGER
%token <strValue>  IDENTIFIER

%token <token> INT8_T INT16_T INT32_T INT64_T
%token <token> UINT8_T UINT16_T UINT32_T UINT64_T
%token <token> BOOL_KW VOID_KW

%token <token> KW_CONSTEXPR
%token <token> KW_IF KW_ELSE KW_FOR KW_WHILE KW_RETURN
%token <token> KW_UNIQUE KW_ALIASED
%token <token> KW_PARALLEL
%token <token> KW_ABS KW_MIN KW_MAX
%token <token> KW_TRUE KW_FALSE

%token <token> ADD SUB MUL
%token <token> LSHIFT RSHIFT
%token <token> ASSIGN ADD_ASSIGN SUB_ASSIGN MUL_ASSIGN
%token <token> EQ NEQ LT GT LE GE
%token <token> AND OR NOT
%token <token> INC DEC
%token <token> REF

%token <token> OPEN_PAREN CLOSE_PAREN
%token <token> OPEN_BRACE CLOSE_BRACE
%token <token> OPEN_BRACKET CLOSE_BRACKET
%token <token> SEMICOLON COMMA

%token <token> IGNORED UNKNOWN

%type <program>       program
%type <declList>      declList
%type <decl>          decl funcDecl constexprFuncDecl constexprVarDecl constexprArrayDecl
%type <typeKind>      type
%type <paramList>     paramList nonEmptyParamList
%type <param>         param
%type <aliasQualifier> aliasQualifier
%type <stmt>          stmt compoundStmt exprStmt localDeclStmt ifStmt forStmt whileStmt returnStmt parallelBlock forInit
%type <stmtList>      stmtList
%type <expr>          expr forCond forUpdate
%type <exprList>      argList nonEmptyArgList exprList

%right ASSIGN ADD_ASSIGN SUB_ASSIGN MUL_ASSIGN
%left  OR
%left  AND
%left  EQ NEQ
%left  LT GT LE GE
%left  LSHIFT RSHIFT
%left  ADD SUB
%left  MUL
%right NOT NEG
%left  INC DEC OPEN_BRACKET

%%

program: declList       { $$ = ProgramSemanticAction($1); }
    ;

declList
    : decl              { $$ = SingleDeclListSemanticAction($1); }
    | declList decl     { $$ = AppendDeclSemanticAction($1, $2); }
    ;

decl
    : funcDecl          { $$ = $1; }
    | constexprFuncDecl { $$ = $1; }
    | constexprVarDecl  { $$ = $1; }
    | constexprArrayDecl { $$ = $1; }
    ;

funcDecl
    : type IDENTIFIER OPEN_PAREN paramList CLOSE_PAREN compoundStmt
        { $$ = FunctionDeclSemanticAction($1, $2, $4, $6); }
    ;

constexprFuncDecl
    : KW_CONSTEXPR type IDENTIFIER OPEN_PAREN paramList CLOSE_PAREN compoundStmt
        { $$ = ConstexprFuncDeclSemanticAction($2, $3, $5, $7); }
    ;

constexprVarDecl
    : KW_CONSTEXPR type IDENTIFIER ASSIGN expr SEMICOLON
        { $$ = ConstexprVarDeclSemanticAction($2, $3, $5); }
    ;

constexprArrayDecl
    : KW_CONSTEXPR type IDENTIFIER OPEN_BRACKET expr CLOSE_BRACKET ASSIGN OPEN_BRACE exprList optComma CLOSE_BRACE SEMICOLON
        { $$ = ConstexprArrayDeclSemanticAction($2, $3, $5, $9); }
    ;

optComma
    : %empty
    | COMMA
    ;

type
    : INT8_T    { $$ = TYPE_INT8; }
    | INT16_T   { $$ = TYPE_INT16; }
    | INT32_T   { $$ = TYPE_INT32; }
    | INT64_T   { $$ = TYPE_INT64; }
    | UINT8_T   { $$ = TYPE_UINT8; }
    | UINT16_T  { $$ = TYPE_UINT16; }
    | UINT32_T  { $$ = TYPE_UINT32; }
    | UINT64_T  { $$ = TYPE_UINT64; }
    | BOOL_KW   { $$ = TYPE_BOOL; }
    | VOID_KW   { $$ = TYPE_VOID; }
    ;

paramList
    : %empty                            { $$ = EmptyParamListSemanticAction(); }
    | nonEmptyParamList                 { $$ = $1; }
    ;

nonEmptyParamList
    : param                             { $$ = SingleParamListSemanticAction($1); }
    | nonEmptyParamList COMMA param     { $$ = AppendParamSemanticAction($1, $3); }
    ;

param
    : type IDENTIFIER
        { $$ = ValueParamSemanticAction($1, $2); }
    | type IDENTIFIER OPEN_BRACKET expr CLOSE_BRACKET
        { $$ = ArrayParamSemanticAction($1, $4, $2); }
    | type REF IDENTIFIER
        { $$ = RefParamSemanticAction($1, QUALIFIER_UNIQUE, $3); }
    | type REF aliasQualifier IDENTIFIER
        { $$ = RefParamSemanticAction($1, $3, $4); }
    ;

aliasQualifier
    : KW_UNIQUE     { $$ = QUALIFIER_UNIQUE; }
    | KW_ALIASED    { $$ = QUALIFIER_ALIASED; }
    ;

compoundStmt
    : OPEN_BRACE stmtList CLOSE_BRACE   { $$ = CompoundStmtSemanticAction($2); }
    ;

stmtList
    : %empty                            { $$ = EmptyStmtListSemanticAction(); }
    | stmtList stmt                     { $$ = AppendStmtSemanticAction($1, $2); }
    ;

stmt
    : compoundStmt      { $$ = $1; }
    | exprStmt          { $$ = $1; }
    | localDeclStmt     { $$ = $1; }
    | ifStmt            { $$ = $1; }
    | forStmt           { $$ = $1; }
    | whileStmt         { $$ = $1; }
    | returnStmt        { $$ = $1; }
    | parallelBlock     { $$ = $1; }
    ;

exprStmt
    : expr SEMICOLON    { $$ = ExprStmtSemanticAction($1); }
    ;

localDeclStmt
    : type IDENTIFIER SEMICOLON
        { $$ = LocalDeclStmtSemanticAction($1, $2, NULL); }
    | type IDENTIFIER ASSIGN expr SEMICOLON
        { $$ = LocalDeclStmtSemanticAction($1, $2, $4); }
    ;

ifStmt
    : KW_IF OPEN_PAREN expr CLOSE_PAREN stmt
        { $$ = IfStmtSemanticAction($3, $5, NULL); }
    | KW_IF OPEN_PAREN expr CLOSE_PAREN stmt KW_ELSE stmt
        { $$ = IfStmtSemanticAction($3, $5, $7); }
    ;

forStmt
    : KW_FOR OPEN_PAREN forInit SEMICOLON forCond SEMICOLON forUpdate CLOSE_PAREN stmt
        { $$ = ForStmtSemanticAction($3, $5, $7, $9); }
    ;

forInit
    : %empty
        { $$ = NULL; }
    | expr
        { $$ = ExprStmtSemanticAction($1); }
    | type IDENTIFIER ASSIGN expr
        { $$ = LocalDeclStmtSemanticAction($1, $2, $4); }
    ;

forCond
    : %empty    { $$ = NULL; }
    | expr      { $$ = $1; }
    ;

forUpdate
    : %empty    { $$ = NULL; }
    | expr      { $$ = $1; }
    ;

whileStmt
    : KW_WHILE OPEN_PAREN expr CLOSE_PAREN stmt
        { $$ = WhileStmtSemanticAction($3, $5); }
    ;

returnStmt
    : KW_RETURN SEMICOLON           { $$ = ReturnStmtSemanticAction(NULL); }
    | KW_RETURN expr SEMICOLON      { $$ = ReturnStmtSemanticAction($2); }
    ;

parallelBlock
    : KW_PARALLEL OPEN_BRACE stmtList CLOSE_BRACE
        { $$ = ParallelBlockSemanticAction($3); }
    ;

expr
    : INTEGER
        { $$ = IntLiteralSemanticAction($1); }
    | KW_TRUE
        { $$ = BoolLiteralSemanticAction(1); }
    | KW_FALSE
        { $$ = BoolLiteralSemanticAction(0); }
    | IDENTIFIER
        { $$ = IdentifierExprSemanticAction($1); }
    | IDENTIFIER OPEN_PAREN argList CLOSE_PAREN
        { $$ = CallExprSemanticAction($1, $3); }
    | KW_ABS OPEN_PAREN expr CLOSE_PAREN
        { ExprList *a = SingleExprListSemanticAction($3); $$ = IntrinsicExprSemanticAction(INTRINSIC_ABS, a); }
    | KW_MIN OPEN_PAREN expr COMMA expr CLOSE_PAREN
        { ExprList *a = AppendExprListSemanticAction(SingleExprListSemanticAction($3), $5); $$ = IntrinsicExprSemanticAction(INTRINSIC_MIN, a); }
    | KW_MAX OPEN_PAREN expr COMMA expr CLOSE_PAREN
        { ExprList *a = AppendExprListSemanticAction(SingleExprListSemanticAction($3), $5); $$ = IntrinsicExprSemanticAction(INTRINSIC_MAX, a); }
    | OPEN_PAREN expr CLOSE_PAREN
        { $$ = $2; }
    | expr ADD expr
        { $$ = BinaryExprSemanticAction(BINOP_ADD, $1, $3); }
    | expr SUB expr
        { $$ = BinaryExprSemanticAction(BINOP_SUB, $1, $3); }
    | expr MUL expr
        { $$ = BinaryExprSemanticAction(BINOP_MUL, $1, $3); }
    | expr LSHIFT expr
        { $$ = BinaryExprSemanticAction(BINOP_LSHIFT, $1, $3); }
    | expr RSHIFT expr
        { $$ = BinaryExprSemanticAction(BINOP_RSHIFT, $1, $3); }
    | expr EQ expr
        { $$ = BinaryExprSemanticAction(BINOP_EQ, $1, $3); }
    | expr NEQ expr
        { $$ = BinaryExprSemanticAction(BINOP_NEQ, $1, $3); }
    | expr LT expr
        { $$ = BinaryExprSemanticAction(BINOP_LT, $1, $3); }
    | expr GT expr
        { $$ = BinaryExprSemanticAction(BINOP_GT, $1, $3); }
    | expr LE expr
        { $$ = BinaryExprSemanticAction(BINOP_LE, $1, $3); }
    | expr GE expr
        { $$ = BinaryExprSemanticAction(BINOP_GE, $1, $3); }
    | expr AND expr
        { $$ = BinaryExprSemanticAction(BINOP_AND, $1, $3); }
    | expr OR expr
        { $$ = BinaryExprSemanticAction(BINOP_OR, $1, $3); }
    | NOT expr
        { $$ = UnaryExprSemanticAction(UNOP_NOT, $2); }
    | SUB expr %prec NEG
        { $$ = UnaryExprSemanticAction(UNOP_NEG, $2); }
    | expr ASSIGN expr
        { $$ = AssignExprSemanticAction($1, ASSIGN_PLAIN, $3); }
    | expr ADD_ASSIGN expr
        { $$ = AssignExprSemanticAction($1, ASSIGN_ADD, $3); }
    | expr SUB_ASSIGN expr
        { $$ = AssignExprSemanticAction($1, ASSIGN_SUB, $3); }
    | expr MUL_ASSIGN expr
        { $$ = AssignExprSemanticAction($1, ASSIGN_MUL, $3); }
    | expr OPEN_BRACKET expr CLOSE_BRACKET
        { $$ = ArrayAccessExprSemanticAction($1, $3); }
    | expr INC
        { $$ = PostIncExprSemanticAction($1); }
    | expr DEC
        { $$ = PostDecExprSemanticAction($1); }
    ;

argList
    : %empty            { $$ = NULL; }
    | nonEmptyArgList   { $$ = $1; }
    ;

nonEmptyArgList
    : expr                          { $$ = SingleExprListSemanticAction($1); }
    | nonEmptyArgList COMMA expr    { $$ = AppendExprListSemanticAction($1, $3); }
    ;

exprList
    : expr                          { $$ = SingleExprListSemanticAction($1); }
    | exprList COMMA expr           { $$ = AppendExprListSemanticAction($1, $3); }
    ;

%%
