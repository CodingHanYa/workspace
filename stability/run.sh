#!/bin/bash

# log result
log_file=./result.txt


print() {
    echo -e -n $@ >> $log_file
}

println() {
    echo -e $@ >> $log_file
}


echo_test_result() {
    println "File: "$1" "$2"/"$3 
}

run_test_file() {

    test_file_name=$1
    success=0

    times=$2
    tmp1=${times}

    tick=$3
    tmp2=$tick

    ptimes=0

    while [ $tmp1 -gt 0 ]
    do
        ./${test_file_name}
        let tmp1--
        if [ $? -eq 0 ]
        then    
            let success++
        fi

        if [[ $success -ge $tmp2 ]] 
        then 
            for (( i=0 ; i <= $ptimes ; i++))
            do
                print "=" 
            done
            println ""
            let tmp2+=${tick}
            let ptimes+=1
        fi

    done 

    echo_test_result $test_file_name $success $times

}

# clean the old result
printf "" > $log_file

g++ test_balance.cpp -o test_balance -lpthread
g++ test_dynamic.cpp -o test_dynamic -lpthread
g++ test_steady.cpp -o test_steady -lpthread

println "====================================="
println "=\t\tSTART STABILITY TEST\t\t="
println "=====================================\n"

println "Your Last Commit Info:"
m1=$(tail -n 1 ../.git/logs/HEAD)
echo $m1 | sed "s/ /\n/2" >> $log_file
println "------------------------------"


run_test_file test_steady  200 10
run_test_file test_balance 200 10
run_test_file test_dynamic 10000 500


println "\n============================="
println "=\t\tEND OF THE TEST\t\t="
println "============================="




