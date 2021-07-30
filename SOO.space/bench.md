# bench for server

## totale time for broadcaste in loopback 

tested in loopback for bench the server not the network. 

connected client = 4

file broadcaste by clients = 50 

file size = 2097152 B (2MB)  

server totale bytes recve = 400MB (100MB by client)
server totale bytes send = 1200MB  (300MB by client)

moyen for one client to send 100MB from server  = 1.5s
moyen for one client to rcv  300MB from server = 1.65s

server input rate = 264MB/s (4 * 66MB/s)
output rate = 727MB/s	   (4 * 181MB/s)

## ram and cpu used by the docker container
tested in loopback for bench the server not the network. 

container only on cpu core 1

connected client = 4

file broadcaste by clients = 50 

file size = 2097152 B (2MB) 

 