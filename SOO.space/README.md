# SOO.space server

## build and run server

Requirement for run the server in docker:

- docker engine.
- docker compose. 

source for install docker : https://docs.docker.com/engine/install

build and run server in docker:

```bash
cd server/docker
./buildRun.sh
```



##  run client demo

### step 1 

build and run the server 

### step 2 

build and test client

``` bash
cd client/code
make run FILE_SIZE=2097152
```

the server receive data from client and the size of data must be FILE_SIZE.

exit client process `CTRL c`

### step 3

run demo 

``` bash
cd client 
./demo.sh
```

### step 4

check demo 

``` bash
cd client
./checkDemo.sh
```

