#!/bin/bash
for i in {1..4}
do
	mkdir client_$i
	cp code/main client_$i/
	cp code/send_data.bin client_$i/
done

for j in {1..4}
do
	cd client_$j
	./main&
	cd ..
done

