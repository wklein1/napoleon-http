# **napoleon-http** 🍰

**napoleon-http** is a modular, cross-platform (WIP) HTTP server in C — inspired by the layered Napoleon cake.     
    Like the cake, the server is built from multiple separate layers that work together and can be swapped or “re-baked” as needed.

Why the name?

🍰 Napoleon cake — no historical reference, just a sweet one.
Across Eastern Europe and Central Asia, the Napoleon cake is a classic made of many thin layers.
That same idea drives this project: entire network stack layers, from link/transport and TLS up through HTTP, routing, and app logic, are modular and can be composed or replaced with custom implementations.

Core features

* **Layered architecture**: clear separation of transport, HTTP parser, routing, and app.

* **Static & API routing**: API router under /api plus multiple static file routers (VFS-backed).

* **Redirects**: small registry to declare path redirects (e.g., / → /docs/, canonical slash).

* **Portable design**: POSIX backend today; designed to port to embedded targets (e.g., ESP-IDF) (WIP).

* **Pluggable layers (planned)**: Pluggable layers (planned): Swap transport/link, TLS, or the VFS via vtables—without touching HTTP or routing (e.g., custom marshaling, non-TCP links, TLS wrappers). 
---

### BUILD & RUN (MAKE)

The repo uses a plain **Makefile** (no CMake).


Build debug (default):     

```make```

Or build release:

```make release```

Remove all generated files:

```make clean```

Run:

```make run```

(optionally pass a port):

```make run args=3001```

Binary paths:
 - ```build/debug/napoleon_httpd```
 - ```build/release/napoleon_httpd```

---

### Preconfigured paths:

- static:
    - /docs/
    - /public/
- api:
    - /api/echo

---
### Quick checks 
```bash

# Redirects
curl -i http://localhost:3001/
curl -i http://localhost:3001/docs

# Static files
curl -i http://localhost:3001/docs/
curl -i http://localhost:3001/public/

# API echo
curl -i -X POST http://localhost:3001/api/echo -d 'hello from POST'

```
---
### Run the server & open in your browser

1. Start the server:
   ```make run```
   (uses port 3001 by default)

2. Open your browser:
   * Go to ```http://localhost:3001/```
     You’ll be redirected to /docs/ and see the docs index page.
   * The example page under /public/ is at http://localhost:3001/public/

3. If you picked a custom port e.g.
   ```make run ARGS=8080```
   Then open http://localhost:8080/
   > **Note:**
   > The server binds to 127.0.0.1 (loopback) by default. You can only access it from the same machine.
   > To test from other devices on your network, change the bind host in your code to "0.0.0.0" (and then visit ```http://<your-ip>:<port>/```).
---
### How a request flows through the server (current design)

1. **Accept & parse**

    The POSIX listener (server.c) accepts a TCP connection and hands the socket to the HTTP core.
    The core reads bytes, the HTTP parser builds an http_request (method, path, headers, body).

2. **HTTP↔App bridge**

    The adapter (adapter_http_app.c) maps http_request → app_request and calls app_handle_client.
    It later maps app_response (or an app-level redirect) back to an http_response.

3. **Redirects (first chance)**

    Inside the app (app.c), a redirect registry is consulted first.
    Examples: / → /docs/ and /docs → /docs/ (canonical trailing slash).
    If a rule matches, the app fills app_response.redirect; the adapter returns a 3xx with Location.

4. **Dynamic routes (/api)**

    If no redirect, the API router (prefix /api) tries to handle the request (e.g., /api/echo).
    On success it returns an app response (status, media type, payload).

5. **Static files (one or more mounts)**

    If still unhandled, the app iterates static routers (e.g., /public, /docs).
    A matching router serves files via the virtual filesystem (VFS):

    Directory requests fall back to index.html (configurable per mount).

    File size is checked against the mount’s max_bytes.

    All file access goes through the VFS (filesystem.c), which calls the active backend (currently POSIX: fs_posix.c).
    The POSIX backend resolves paths under the configured root and blocks .. traversal.

6. **Serialize & send**

    The adapter converts the app response to HTTP (status, Content-Type via http_mime.c, headers, body).
    The HTTP core writes headers + body with Content-Length and Connection: close, then closes the socket.
