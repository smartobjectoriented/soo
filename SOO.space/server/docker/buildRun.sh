#!/bin/bash

sudo docker kill $(sudo docker ps -a -q)
sudo docker rm $(sudo docker ps -a -q)
sudo docker build --network=host -t soo/space .
sudo docker run -p 7070:7070 --network=host --cpuset-cpus=1 soo/space 
