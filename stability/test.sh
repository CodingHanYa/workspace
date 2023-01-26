#!/bin/bash

# if should make firstly

echo "==============================="
echo "*    Start Stability Test     *"
echo "==============================="

times=10000
success=0
tick=10

echo "Total test times: "$times 
echo "------>"

tmp1=${times}
tmp2=${tick}

while [[ $tmp1 > 0 ]]
do
    ./test
	let tmp1--
    if [ $? -eq 0 ] 
    then 
        let success++
    fi
    if [ $success -gt $tmp2 ] 
    then 
        echo "Passed "${tmp2}" examples ... "
        let tmp2+=${tick}
    fi
done

echo "<------"
echo "passing rate = "${success}"/"${times}" !"


echo "================================="
echo "*        End of the test        *"
echo "================================="
