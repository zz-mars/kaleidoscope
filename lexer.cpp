#include <cctype>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string>
#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

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
