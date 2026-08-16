// Benchmark comparison server (Phase 38.2) — mirrors Aurora /hello endpoint.
// Run:  node bench_http_node.js   (listens on :8081)
const http = require("http");

const server = http.createServer((req, res) => {
  if (req.url === "/hello") {
    res.writeHead(200, { "Content-Type": "application/json" });
    res.end('{"hello":"world","n":1,"ok":true}');
  } else if (req.url === "/text") {
    res.writeHead(200, { "Content-Type": "text/html" });
    res.end("hello");
  } else {
    res.writeHead(404);
    res.end("not found");
  }
});

server.listen(8081, () => {
  console.log("Node.js benchmark server on :8081");
});