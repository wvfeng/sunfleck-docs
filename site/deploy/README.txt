Sunfleck documentation — static deployment

Extract the complete archive into the server document root or a subdirectory.
Keep index.html, JS, Wasm and preloaded font data from the same build together.
Do not open index.html via file://. Use HTTP(S).

For local preview from this application:
  python -m http.server 8790 --bind 127.0.0.1 --directory out/web

For Nginx use nginx.example.conf inside your HTTPS server configuration.
Serve .wasm as application/wasm and .js as text/javascript.
If serving .gz variants, use Content-Encoding: gzip and Vary: Accept-Encoding.
Revalidate shared resources. Deploy a fresh directory and atomically switch the
server root/symlink; keep the previous complete build for rollback.
Routes such as #/Button and #/quick work unchanged at / or /docs/.

resources.json records every uncompressed asset's SHA-256. Keep licenses/ with
the distribution. No upload, server credentials or automatic publication is
performed by the build tooling.
