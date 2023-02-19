tar = test
flag = -g -Wall -O2 -std=c++17
lib = -lpthread  

# flag: -std=c++17 for BS
test_file1 = ./test_empty_task.cpp
test_file2 = ./test_speedup.cpp
test_file3 = ./compare_batch_submit.cpp
test_file4 = ./compare_submit.cpp
test_file5 = ./compare_other_task.cpp

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

