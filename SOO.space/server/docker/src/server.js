const net = require('net');
const { tmpdir } = require('os');
const port = 7070;
const time_out = 360000;
var buffer = new Object(); 
var ME_size = new Object();
var ME_rcv_byt = new Object();
var me_rcv_time = new Object();
var timeOutCheck = new Uint32Array(2);
timeOutCheck[0] = 0x4;
timeOutCheck[1] = time_out;
var cmdBuffer = Buffer.from(timeOutCheck.buffer)


let sockets = [];



const server = net.createServer();
server.listen(port,() => {
    console.log('TCP Server is running on  ',server.address());
});


server.on('connection', function(sock) {
    console.log('CONNECTED: ' + sock.remoteAddress + ':' + sock.remotePort);
    
    sock.setTimeout(time_out);

    sockets.push(sock);

    //start buffer
    buffer[sock.remotePort + sock.remoteAddress] = [];
    

    sock.on('data', function(data) {

        let source = sock.remotePort + sock.remoteAddress;
        let save = Buffer.from('','utf8');
	
	
        //if new ME read the size in the first 4 bytes
        if(buffer[source].length == 0){
            let size = data.readUInt32LE(0);
            console.log("new ME from:" + sock.remoteAddress + " size:" + size + " KB")
            me_rcv_time[source] = Date.now();
            ME_size[source] = size + 4;
            ME_rcv_byt[source] = 0;
            
        }
        
        //if data chunk contien only one ME
        if(data.length <= ME_size[source]){

            //save data to buffer
            var newBuffer = Buffer.alloc(data.length);
            data.copy(newBuffer);
            buffer[source].push(newBuffer);

            //update the current number of bytes to read
            ME_size[source] = ME_size[source] - data.length;
          
        }else{

            //if data chunk contien 2 ME 
            //save current ME to buffer
            var newBuffer = Buffer.alloc(ME_size[source]);
            data.copy(newBuffer,0,0,ME_size[source]);
            buffer[source].push(newBuffer);

            //save next MEs to new buffer
            save = new Buffer.alloc(data.length - ME_size[source]);
            data.copy(save,0,ME_size[source]);
            ME_size[source] = 0;
        }

        //if all bytes of current ME is rcv
        if( ME_size[source] == 0){
	    console.log("time for rcv ME = "+(Date.now() - me_rcv_time[source])+"ms");
            sockets.forEach(function(sock, index, array) {
                // Write the data back to all the connected
                if(sock.remotePort + sock.remoteAddress != source){
                    var size_data = 0;

                    buffer[source].forEach(function(data,index,array){
                        size_data += data.length;
                        sock.write(data);
                    });

                    console.log(source + " send :" +  size_data  +"B to " + sock.remotePort + ':' + sock.remoteAddress);
                }
            });
            //clear buffer of smart object
            buffer[source].length = 0;

            //if next ME is in the same chunk
            if(save.length > 0){
                
                //read the size in the first 4 bytes
                let size = save.readUInt32LE(0);
		 console.log("new ME from " + sock.remoteAddress + " size: " + size + " KB")
		  me_rcv_time[source] = Date.now();
                //update the current number of bytes to read
                ME_size[source] = size - save.length + 4;
                //and put next ME in buffer
                buffer[source].push(save);
            }
        }    
    });
    

    // Add a 'close' event handler to this instance of socket
    sock.on('close', function(data) {
        let index = sockets.findIndex(function(o) {
            return o.remoteAddress === sock.remoteAddress && o.remotePort === sock.remotePort;
        })
        if (index !== -1) sockets.splice(index, 1);
        console.log('CLOSED: ' + sock.remoteAddress + ' ' + sock.remotePort);
        let source = sock.remotePort + sock.remoteAddress;
        buffer[source].length = 0;
    });


  sock.on("error", function(err){
        console.log("Caught flash policy server socket error: ");
        console.log(err.stack);
        sock.destroy();
  });

  //if smart Object dont send data check if it still alive
  sock.on('timeout', () => {
        sock.write(cmdBuffer);
  });

});
