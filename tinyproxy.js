// tinyproxy.js
const http = require("http");
const https = require("https");
const { URL } = require("url");
const zlib = require("zlib");

function isAbsoluteUrl(s) {
  return s.startsWith("http://") || s.startsWith("https://");
}

const server = http.createServer((req, res) => {
  try {

    const targetStr = isAbsoluteUrl(req.url)
      ? req.url
      : `http://${req.headers.host || "example.com"}${req.url || "/"}`;

    const target = new URL(targetStr);
    const lib = target.protocol === "https:" ? https : http;

    const upstreamReq = lib.request(

      {
        protocol: target.protocol,
        hostname: target.hostname,
        port: target.port || (target.protocol === "https:" ? 443 : 80),
        method: req.method,
        path: target.pathname + target.search,
        headers: {
          "User-Agent": "Tiny64-Proxy",
          "Accept": "*/*",
          "Accept-Encoding": "gzip, deflate, br",
          "Connection": "close",
          "Host": target.host,
        },
      },
      (upstreamRes) => {
        const chunks = [];
        let total = 0;
        const enc = String(upstreamRes.headers["content-encoding"] || "").toLowerCase();
        console.log(`[proxy] ${req.method} ${targetStr} -> ${upstreamRes.statusCode} enc=${enc || "none"}`);
        upstreamRes.on("data", (c) => {
          chunks.push(c);
          total += c.length;
        });
        upstreamRes.on("end", () => {
          let body = Buffer.concat(chunks, total);

          try {
            if (enc === "gzip") body = zlib.gunzipSync(body);
            else if (enc === "deflate") body = zlib.inflateSync(body);
            else if (enc === "br") body = zlib.brotliDecompressSync(body);
          } catch (e) {
            res.writeHead(502, { "Content-Type": "text/plain", "Connection": "close" });
            res.end(`proxy decode error (${enc}): ${e.message}\n`);
            return;
          }

          const headers = { ...upstreamRes.headers };

          delete headers["transfer-encoding"];
          delete headers["content-encoding"];
          delete headers["content-length"];
          delete headers["connection"];

          headers["content-length"] = String(body.length);
          headers["connection"] = "close";

          res.writeHead(upstreamRes.statusCode || 502, headers);
          res.end(body);
        });
      }
    );

    upstreamReq.on("error", (e) => {
      res.writeHead(502, { "Content-Type": "text/plain", "Connection": "close" });
      res.end(`proxy upstream error: ${e.message}\n`);
    });

    req.pipe(upstreamReq);
  } catch (e) {
    res.writeHead(400, { "Content-Type": "text/plain", "Connection": "close" });
    res.end(`proxy bad request: ${e.message}\n`);
  }
});

// Listen on all interfaces so QEMU user-net can reach it via 10.0.2.2
server.listen(3128, "0.0.0.0", () => {
  console.log("Tiny proxy listening on http://0.0.0.0:3128");
});