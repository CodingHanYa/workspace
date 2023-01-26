tar = test
flag = -g -std=c++11
lib = -lpthread  

test_file1 = ./test_steady.cpp
test_file2 = ./test_dynamic.cpp

src = ${test_file2}

${tar}: ${src}
	g++ ${flag} ${src} -o ${tar} ${lib}

.PRONY: exec clean

clean:
	@ rm ./${tar}

exec:
	@make clean
	@make
	@./${tar}
