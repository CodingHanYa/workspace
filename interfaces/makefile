tar = test
flag = -g -std=c++11
lib = -lpthread  

test_file1 = ./test_steady_pond_interface.cpp
test_file2 = ./test_dynamic_pond_interface.cpp
test_file3 = ./test_balanced_pond_interface.cpp

src = ${test_file3}

${tar}: ${src}
	g++ ${flag} ${src} -o ${tar} ${lib}

.PRONY: exec clean

clean:
	@ rm ./${tar}

exec:
	@make clean
	@make
	@./${tar}

