**napoleon-http** 🍰

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
