tar = test
flag = -g -std=c++17
lib = -lpthread  

# flag: -std=c++17 for BS
test_file1 = ./test_empty_task.cpp
test_file2 = ./test_speedup.cpp

${tar}: ${src}
	g++ ${flag}  ${test_file1} -o ${tar} ${lib}

.PRONY: exec clean

clean:
	@ rm ./${tar}

exec:
	@make clean
	@make
	@./${tar}

