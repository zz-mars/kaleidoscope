#include <cctype>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string>
#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "llvm/ADT/STLExtras.h"
#include <cstdio>
#include <map>
#include <vector>

// the lexer return [0-255] if it is an unknown character,
// otherwise one of these for known things
enum Token {
	tok_eof = -1,
	tok_def = -2,
	tok_extern = -3,
	tok_identifier = -4,
	tok_number = -5,
};

// for tok_identifier
static std::string IdentifierStr;
// for tok_number
static double NumVal;

// return the next token from stdin
static int gettok() {
	static int LastChar = ' ';
	// eat spaces
	while(isspace(LastChar)) {
		LastChar = getchar();
	}
	// recognize identifiers
	if(isalpha(LastChar)) {
		IdentifierStr = LastChar;
		while(isalnum((LastChar = getchar()))) {
			IdentifierStr += LastChar;
		}
		if(IdentifierStr == "def") {
			return tok_def;
		}
		if(IdentifierStr == "extern") {
			return tok_extern;
		}
		return tok_identifier;
	}
	// recognize number
	bool DotFound = LastChar=='.';
	if(isdigit(LastChar) || DotFound) {
		std::string NumStr;
		do {
			NumStr += LastChar;
			LastChar = getchar();
		} while(isdigit(LastChar) || (!DotFound && LastChar=='.' && (DotFound=true)));
		NumVal = strtod(NumStr.c_str(), 0);
		return tok_number;
	}
	// anotations
	if(LastChar == '#') {
		do {
			LastChar = getchar();
		} while(LastChar != EOF && LastChar != '\n' && LastChar != '\r');
		// not end of file, get next token
		if(LastChar != EOF) {
			return gettok();
		}
	}
	// end of file
	if(LastChar == EOF) {
		return tok_eof;
	}
	int ThisChar = LastChar;
	// for next calling of gettok
	LastChar = getchar();
	// return the ascii
	return ThisChar;
}

#ifdef _TEST_LEXER_
int main(int argc, char * argv[]) {
	if(argc != 2) {
		printf("Usage: %s <file_to_pass>\n", argv[0]);
		exit(1);
	}
	char * inputfile = argv[1];
	int fd = open(inputfile, O_RDONLY);
	if(fd < 0) {
		perror("open file error");
		exit(1);
	}
	// read file from stdin
	if(dup2(fd, 0) < 0) {
		perror("dup2 error");
		exit(1);
	}
	for(;;) {
		int tok = gettok();
		switch(tok) {
			case tok_eof:
				std::cout << "end of file found" << std::endl;
				exit(0);
			case tok_def:
				std::cout<< "tok_def -> " << IdentifierStr << std::endl;
				break;
			case tok_extern:
				std::cout << "tok_extern -> " << IdentifierStr << std::endl;
				break;
			case tok_identifier:
				std::cout << "tok_identifier -> " << IdentifierStr << std::endl;
				break;
			case tok_number:
				std::cout << "tok_number -> " << NumVal << std::endl;
				break;
			default:
				char tok_ch = tok;
				std::cout << "default token -> " << tok_ch << std::endl;
				break;
		}
	}
	exit(0);
}
#endif

namespace {
class ExprAST {
	public:
		virtual ~ExprAST() {}
};

class NumberExprAST : public ExprAST {
	double Val;
	public:
		NumberExprAST(double Val) : Val(Val) {}
};

class VariableExprAST : public ExprAST {
	std::string Name;
	public:
		VariableExprAST(const std::string &Name) : Name(Name) {}
};

class BinaryExprAST : public ExprAST {
	char Op;
	std::unique_ptr<ExprAST> LHS, RHS;
	public:
		BinaryExprAST(char Op, std::unique_ptr<ExprAST> LHS,
				std::unique_ptr<ExprAST> RHS) :
			Op(Op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}
};

class CallExprAST : public ExprAST {
	std::string Callee;
	std::vector<std::unique_ptr<ExprAST>> Args;
	public:
		CallExprAST(const std::string &Callee,
				std::vector<std::unique_ptr<ExprAST>> Args) :
			Callee(Callee), Args(std::move(Args)) {}
};

class PrototypeAST {
	std::string Name;
	std::vector<std::string> Args;
	public:
		PrototypeAST(const std::string &Name, std::vector<std::string> Args) :
			Name(Name), Args(std::move(Args)) {}
};

class FunctionAST {
	std::unique_ptr<PrototypeAST> Proto;
	std::unique_ptr<ExprAST> Body;
	public:
		FunctionAST(std::unique_ptr<PrototypeAST> Proto,
				std::unique_ptr<ExprAST> Body) :
			Proto(std::move(Proto)), Body(std::move(Body)) {}
};
} // end of anonymous namespace

// parser
static int CurTok;
static int getNextToken() {
	return CurTok = gettok();
}

// precedence table
static std::map<char, int> BinopPrecedence;

static int GetTokPrecedence() {
	if(!isascii(CurTok)) {
		return -1;
	}
	int TokPrec = BinopPrecedence[CurTok];
	return TokPrec<=0?-1:TokPrec;
}

// error handling helper
std::unique_ptr<ExprAST> Error(const char *Str) {
	fprintf(stderr, "Error: %s\n", Str);
	return nullptr;
}
std::unique_ptr<PrototypeAST> ErrorP(const char *Str) {
	fprintf(stderr, "PError: %s\n", Str);
	return nullptr;
}

// declared here, implemented later
static std::unique_ptr<ExprAST> ParseExpression();
// parse Number Expression
static std::unique_ptr<ExprAST> ParseNumberExpr() {
	auto Result = llvm::make_unique<NumberExprAST>(NumVal);
	getNextToken(); // eat the number
	return std::move(Result);
}
// parse parences : '(' expression ')'
static std::unique_ptr<ExprAST> ParseParenExpr() {
	getNextToken(); // eat '('
	auto V = ParseExpression();
	if(!V) {
		return nullptr;
	}
	if(CurTok != ')') {
		return Error("expected ')'");
	}
	getNextToken(); // eat ')'
	return V;
}
// parse identifier
static std::unique_ptr<ExprAST> ParseIdentifierExpr() {
	std::string IdName = IdentifierStr;
	getNextToken();
	// simple variable reference
	if(CurTok != '(') {
		return llvm::make_unique<VariableExprAST>(IdName);
	}
	// function call
	getNextToken();
	std::vector<std::unique_ptr<ExprAST>> Args;
	if(CurTok != ')') {
		while(1) {
			if(auto Arg = ParseExpression()) {
				Args.push_back(std::move(Arg));
			} else {
				return nullptr;
			}
			if(CurTok == ')') {
				break;
			}
			if(CurTok != ',') {
				return Error("Expected ')' or ',' in argument list");
			}
			getNextToken();
		}
	}
	getNextToken(); // eat ')'
	return llvm::make_unique<CallExprAST>(IdName, std::move(Args));
}

static std::unique_ptr<ExprAST> ParsePrimary() {
	switch(CurTok) {
		case tok_identifier:
			return ParseIdentifierExpr();
		case tok_number:
			return ParseNumberExpr();
		case '(':
			return ParseParenExpr();
		default:
			return Error("unknow token when expecting an expression");
	}
}

static std::unique_ptr<ExprAST> ParseBinOpRHS(int ExprPrec,
		std::unique_ptr<ExprAST> LHS) {
	while(1) {
		// precedence of current token
		int TokPrec = GetTokPrecedence();
		// return LHS if previous operator binds tighter
		if(TokPrec < ExprPrec) {
			return LHS;
		}

		int BinOp = CurTok;
		getNextToken(); // eat binop

		auto RHS = ParsePrimary();
		if(!RHS) {
			return nullptr;
		}
		int NextPrec = GetTokPrecedence();
		if(TokPrec < NextPrec) {
			RHS = ParseBinOpRHS(TokPrec+1, std::move(RHS));
			if(!RHS) {
				return nullptr;
			}
		}
		LHS = llvm::make_unique<BinaryExprAST>(BinOp, std::move(LHS), std::move(RHS));
	}
}

// parse expression
static std::unique_ptr<ExprAST> ParseExpression() {
	auto LHS = ParsePrimary();
	if(!LHS) {
		return nullptr;
	}
	return ParseBinOpRHS(0, std::move(LHS));
}

// parse prototype
static std::unique_ptr<PrototypeAST> ParsePrototype() {
	if(CurTok != tok_identifier) {
		return ErrorP("Expected function name in prototype");
	}
	std::string FnName = IdentifierStr;
	getNextToken();
	if(CurTok != '(') {
		return ErrorP("expected '(' in prototype");
	}
	std::vector<std::string> ArgNames;
	while(getNextToken() == tok_identifier) {
		ArgNames.push_back(IdentifierStr);
	}
	if(CurTok != ')') {
		return ErrorP("expected ')' in prototype");
	}
	getNextToken();
	return llvm::make_unique<PrototypeAST>(FnName, std::move(ArgNames));
}

// def xxx
static std::unique_ptr<FunctionAST> ParseDefinition() {
	getNextToken(); // eat def
	auto Proto = ParsePrototype();
	if(!Proto) {
		return nullptr;
	}
	if(auto E = ParseExpression()) {
		return llvm::make_unique<FunctionAST>(std::move(Proto), std::move(E));
	}
	return nullptr;
}

static std::unique_ptr<FunctionAST> ParseTopLevelExpr() {
	if(auto E = ParseExpression()) {
		// make an anonymous proto
		auto Proto = llvm::make_unique<PrototypeAST>("__anon_expr", std::vector<std::string>());
		return llvm::make_unique<FunctionAST>(std::move(Proto), std::move(E));
	}
	return nullptr;
}

static std::unique_ptr<PrototypeAST> ParseExtern() {
	getNextToken(); // eat extern
	return ParsePrototype();
}

// top level parsing
static void HandleDefinition() {
	if(ParseDefinition()) {
		fprintf(stderr, "Parsed a function definition\n");
	} else {
		getNextToken();
	}
}

static void HandleExtern() {
	if(ParseExtern()) {
		fprintf(stderr, "Parsed an extern\n");
	} else {
		getNextToken();
	}
}

static void HandleTopLevelExpression() {
	if(ParseTopLevelExpr()) {
		fprintf(stderr, "Parsed a top-levle expr\n");
	} else {
		getNextToken();
	}
}

static void MainLoop() {
	while(1) {
		fprintf(stderr, "ready> ");
		switch(CurTok) {
			case tok_eof:
				return;
			case ';':
				getNextToken();
				break;
			case tok_def:
				HandleDefinition();
				break;
			case tok_extern:
				HandleExtern();
				break;
			default:
				HandleTopLevelExpression();
				break;
		}
	}
}

int main() {
	BinopPrecedence['<'] = 10;
	BinopPrecedence['+'] = 20;
	BinopPrecedence['-'] = 20;
	BinopPrecedence['*'] = 40;

	fprintf(stderr, "ready> ");
	getNextToken();
	MainLoop();
	return 0;
}
