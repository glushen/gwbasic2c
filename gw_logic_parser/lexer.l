%option noyywrap nodefault

%{
#include <gw_logic_parser.h>

int lexer_read_char_count;
int lexer_signature_offset;

#define YY_USER_INIT    lexer_read_char_count = 0; \
                        lexer_signature_offset = -1;
#define YY_USER_ACTION  lexer_read_char_count += yyleng;

void lexer_on_type() {
    if (lexer_signature_offset == -1) {
        lexer_signature_offset = lexer_read_char_count - yyleng;
    }
}
%}

%%

\#include       { return INCLUDE_DIRECTIVE; }
\"[^\"]*\"      { yylval.include_path = new std::string(yytext, 1, yyleng - 2); return INCLUDE_PATH; }

gw_int                                { lexer_on_type(); yylval.type_name = new std::string("INT"); return CONST_TYPE; }
float                                 { lexer_on_type(); yylval.type_name = new std::string("FLOAT"); return CONST_TYPE; }
double                                { lexer_on_type(); yylval.type_name = new std::string("DOUBLE"); return CONST_TYPE; }
std::string|const\ +std::string\ *\&  { lexer_on_type(); yylval.type_name = new std::string("STRING"); return CONST_TYPE; }
gw_int\ *\&                           { lexer_on_type(); yylval.type_name = new std::string("INT_REF"); return REF_TYPE; }
float\ *\&                            { lexer_on_type(); yylval.type_name = new std::string("FLOAT_REF"); return REF_TYPE; }
double\ *\&                           { lexer_on_type(); yylval.type_name = new std::string("DOUBLE_REF"); return REF_TYPE; }
std::string\ *\&                      { lexer_on_type(); yylval.type_name = new std::string("STRING_REF"); return REF_TYPE; }
void                                  { lexer_on_type(); yylval.type_name = new std::string("VOID"); return VOID_TYPE; }

\,|\(|\)                { return yytext[0]; }

[a-zA-Z][a-zA-Z0-9\$\_]*  { yylval.name = new std::string(yytext); return NAME; }

\r\n|\r|\n      { return '\n'; }
[ \t]           { /* ignore */ }
.               { yyerror("Unexpected character %c", yytext[0]); }

%%
