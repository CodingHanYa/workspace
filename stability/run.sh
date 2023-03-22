#!/bin/bash

# log result
log_file=./result.txt

print() {
    echo -e -n "$@" >> $log_file
}

println() {
    echo -e "$@" >> $log_file
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
        if [ $? -eq 0 ]
        then    
            let success++
        fi
        let tmp1--

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

g++ -O2 -g -Wall -Werror -I ../include test_balance.cpp -o test_balance -lpthread
g++ -O2 -g -Wall -Werror -I ../include test_dynamic.cpp -o test_dynamic -lpthread
g++ -O2 -g -Wall -Werror -I ../include test_steady.cpp -o test_steady -lpthread

daildate=`date`
time=`echo $daildate | cut -f4 -d' '`

echo "========================================"
echo -e "          Hipe Stability Test"
echo "========================================"
echo "LogFile: ./Hipe/stability/result.txt"
echo "LogTime: "$time
echo "......"

println "====================================="
println "=        START STABILITY TEST       ="
println "=====================================\n"

println "Your Last Commit Info:"
m1=$(tail -n 1 ../.git/logs/HEAD)
echo $m1 | sed "s/ /\n/2" >> $log_file
println "------------------------------"


run_test_file test_dynamic 10000 500
run_test_file test_steady  1000 50
run_test_file test_balance 1000 50


println "\n=========================="
println "=     END OF THE TEST     ="
println "==========================="

echo -e "================= END =================="




