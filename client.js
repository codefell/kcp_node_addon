const kcp = require("./build/Release/kcp_addon");
const dgram = require("dgram");

let sock = dgram.createSocket("udp4");

let port = 8080;
let address = "127.0.0.1";

let kcpsession = kcp.kcp_create(123, (buff) => {
    sock.send(buff, port, address); 
});

sock.on("message", (message, rinfo) => {
    kcp.kcp_input(kcpsession, message);
});

let index = 100;
setInterval(() => {
    kcp.kcp_send(kcpsession, Buffer.from("this client " + index++));
}, 100);
setInterval(() => {
    let recvBuff = kcp.kcp_recv(kcpsession);
    if (recvBuff) {
        let msg = recvBuff.toString();
        console.log("recv " + msg);
    }
    kcp.kcp_update(kcpsession, Date.now());
}, 20);

