# Fuzzing Harnesses for Napoleon HTTP

This folder contains fuzzing harnesses and a Makefile for fuzzing different layers of the Napoleon HTTP project using [AFL++](https://github.com/AFLplusplus/AFLplusplus).

---

###  Prerequisites

- **Docker Engine** must be installed and running.

  If you get permission errors, add your user to the Docker group and restart your session:

  ```sh
  sudo usermod -aG docker $USER
  newgrp docker
  ```

---

###  Harnesses

We provide three fuzzing harnesses:

- **Unit harness (`unit_parse_harness`)**  
  Focuses on parsing HTTP request lines and headers.

- **Integration harness (`int_parse_harness`)**  
  Parses a full HTTP request, incl. body, using pipes to feed data.

- **End-to-End harness (`e2e_server_harness`)**  
  Starts a server context and processes a full request and response cycle via `socketpair()`. This covers routing, static files, redirects, and API handling.

Each harness is compiled with AFL++ instrumentation and can also run with CmpLog mode for enhanced comparison coverage.

---

###  Build

The Makefile is configured to run inside the official AFL++ Docker image.  
All build artifacts (incl. binaries) will appear under `fuzz/build/`.

To build all harnesses (unit, integration, e2e):

```sh
make all
```

You can also build individually:

```sh
make build-parse-unit
make build-parse-int
make build-server-e2e
```

---

###  Run Fuzzing

Unit harness
```sh
make run-parse-unit
```

 Integration harness
```sh
make run-parse-int
```

 End-to-End harness
```sh
make run-server-e2e
```

Each run will mount your project into a Docker container with AFL++, mount a tmpfs at `/ramdisk`, and execute the fuzzing.

Outputs are stored under `fuzz/out/<unit|int|e2e>/`.

---

### CmpLog Mode

CmpLog mode uses a separate comparison-logging binary to improve coverage.  
You can run any harness with CmpLog enabled:

```sh
make run-parse-unit-cmplog
make run-parse-int-cmplog
make run-server-e2e-cmplog
```

---

### Seeds and Dictionaries

- Place initial seed inputs in `fuzz/seeds/<unit|int|e2e>/`.
- Place custom dictionaries in `fuzz/dicts/`.
              
             

---

### Cleanup

To remove all build artifacts:
```sh
make clean
```

---

### Notes

- All harnesses support two modes:
  - **Fuzzing mode** with AFL++ macros (`__AFL_LOOP`, `__AFL_FUZZ_INIT`, etc.).
  - **Fallback mode** which reads a single test case from `stdin`.  
    This allows you to replay crashes easily:

    ```sh
    cat crash-input | ./build/int_parse_harness
    ```

- Timeouts are configured via the Makefile (`TIMEOUT ?= 1000+`).

