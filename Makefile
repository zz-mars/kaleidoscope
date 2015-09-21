all:
	clang++ -D_TEST_LEXER_ --std=c++11 -stdlib=libc++ lexer.cpp -lc++abi

clean:
	rm a.out
