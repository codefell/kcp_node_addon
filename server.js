const kcp = require("./build/Release/kcp_addon");
const dgram = require("dgram");

let sock = dgram.createSocket("udp4");

let port;
let address;

let kcpsession = kcp.kcp_create(123, (buff) => {
    sock.send(buff, port, address);
});

sock.on("message", (message, rinfo) => {
    port = rinfo.port;
    address = rinfo.address;
    kcp.kcp_input(kcpsession, message);
});

setInterval(() => {
    let recvBuff = kcp.kcp_recv(kcpsession);
    if (recvBuff) {
        let msg = recvBuff.toString();
        console.log("recv " + msg);
        kcp.kcp_send(kcpsession, Buffer.from("RE-" + msg));
    }
    kcp.kcp_update(kcpsession, Date.now());
}, 20);

sock.bind(8080);



