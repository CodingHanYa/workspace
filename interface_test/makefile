tar = test
flag = -g -std=c++11
lib = -lpthread  

test_file1 = ./test_steady_pond_interface.cpp
test_file2 = ./test_dynamic_pond_interface.cpp

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

