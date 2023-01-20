tar = test
flag = -g -std=c++17
lib = -lpthread  

# flag: -std=c++17 for BS
test_file1 = ./test_empty_task.cpp
test_file2 = ./test_speedup.cpp

src = ${test_file1}

${tar}: ${src}
	g++ ${flag} ${src} -o ${tar} ${lib}

.PRONY: exec clean

clean:
	@ rm ./${tar}

exec:
	@make clean
	@make
	@./${tar}

