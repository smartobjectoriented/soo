#!/bin/bash


for i in {1..4}
do
	cd client_$i
	for j in {0..149}
	do
		cmp send_data.bin rcv_data.bin$j >> ../error.log
	done
	cd ..
	rm -rf client_$i
done

if [ -s error.log ]
then
        cat error.log
else
        rm error.log
        echo "0 error detected"
fi

