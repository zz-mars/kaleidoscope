all:
	clang++ --std=c++11 -stdlib=libc++ lexer.cpp -lc++abi

clean:
	rm a.out
