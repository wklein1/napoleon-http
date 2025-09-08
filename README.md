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
### Run the server & open in your browser

1. Start the server:
   ```make run```
   (uses port 3001 by default)

2. Open your browser:
   * Go to ```http://localhost:3001/```
     You’ll be redirected to /docs/ and see the docs index page.
   * The example page under /public/ is at http://localhost:3001/public/

3. If you picked a custom port
   ```make run ARGS=8080```
   Then open http://localhost:8080/
   > **Note:**
   > The server binds to 127.0.0.1 (loopback) by default. You can only access it from the same machine.
   > To test from other devices on your network, change the bind host in your code to "0.0.0.0" (and then visit http://<your-ip>:<port>/).
