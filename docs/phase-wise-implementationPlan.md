# Gateway (GW) Network Packet Communication
## Detailed Phase-Wise Implementation Plan (Dual-Platform: Linux & Windows)

---

### Document Overview
This document outlines the execution roadmap for building the Gateway (GW) Network Packet Communication System as specified in [problemStatement.md](file:///d:/GW%20Network%20packet%20communication/docs/problemStatement.md) and designed in [architecture.md](file:///d:/GW%20Network%20packet%20communication/docs/architecture.md).

The project is structured into **6 progressive development phases**. Each phase details specific implementation tasks, file deliverables, cross-platform requirements (Windows and Linux), formal **Acceptance Criteria**, and **Exit Criteria** required before proceeding to subsequent phases.

---

## Cross-Platform Engineering Strategy (Linux & Windows)

The system must compile cleanly, link without external heavy dependencies, and pass all functional and stress tests natively on both:
1. **Linux**: Ubuntu 20.04/22.04+ (GCC 9+, Clang 11+, POSIX sockets, `pthreads`, CMake).
2. **Windows**: Windows 10/11 / Windows Server (MSVC 2019/2022 or MinGW-w64, Winsock2 `ws2_32.lib`, Win32 Threads / C11 threads, CMake).

### Cross-Platform Abstraction Matrix

| OS Subsystem | Linux Implementation | Windows Implementation | Abstraction Wrapper (`platform.h`) |
| :--- | :--- | :--- | :--- |
| **Socket Types** | `int` file descriptor | `SOCKET` (`UINT_PTR`) | `gw_socket_t` |
| **Socket Error Checking** | `< 0` (errno) | `== SOCKET_ERROR` (`WSAGetLastError()`) | `gw_socket_is_valid()`, `gw_socket_last_error()` |
| **Socket Initialization** | None required | `WSAStartup(&wsaData, ...)` | `gw_platform_init()` |
| **Socket Cleanup** | `close(fd)` | `closesocket(sock)`, `WSACleanup()` | `gw_socket_close()`, `gw_platform_cleanup()` |
| **TCP Broken Pipe** | Handle `SIGPIPE` / `MSG_NOSIGNAL` | Windows does not raise `SIGPIPE` | Handled internally in `platform.c` |
| **Thread Type** | `pthread_t` | `HANDLE` | `gw_thread_t` |
| **Thread Creation** | `pthread_create(...)` | `CreateThread(...)` / `_beginthreadex` | `gw_thread_create(...)` |
| **Thread Joining** | `pthread_join(...)` | `WaitForSingleObject(...)` + `CloseHandle` | `gw_thread_join(...)` |
| **Sleep / Delay** | `usleep()` / `nanosleep()` | `Sleep(milliseconds)` | `gw_sleep_ms(...)` |
| **Byte Order** | `<arpa/inet.h>` (`htons`, `ntohl`) | `<winsock2.h>` (`htons`, `ntohl`) | Unified macros in `platform.h` |

---

## Phase 1 — Foundation & Core Infrastructure

### 1.1 Objectives
Set up the multi-target CMake workspace, build the platform abstraction layer (supporting both Winsock2 and POSIX), define unified protocol headers, and establish the structured logging subsystem and Unity test harness.

### 1.2 Implementation Steps & Tasks

- [ ] **Task 1.1: Root CMake Configuration (`CMakeLists.txt`)**
  - Configure C11 standard enforcement across compilers (`-std=c11` on GCC/Clang, `/std:c11` on MSVC).
  - Add conditional linkage: link `ws2_32.lib` on Windows; link `pthread` on Linux.
  - Define 4 build targets:
    1. `gw_common` (Static library: platform, logger, crc8, crypto, transform, parser, framer).
    2. `gw_server` (Executable: server daemon, socket listener, validation, responses).
    3. `gw_client` (Executable: diagnostic simulator, request generator, response decoder).
    4. `gw_tests` (Test runner using Unity).
  - Enable compiler warnings (`-Wall -Wextra -Werror` on GCC/Clang, `/W4 /WX` on MSVC).

- [ ] **Task 1.2: Protocol Definitions & Config (`include/common/gw_types.h`, `gw_config.h`)**
  - Define packet struct (`gw_packet_t`), failure codes enum (`gw_failure_code_t`), and response types.
  - Set canonical protocol constants: port `13400`, valid addresses (`0x0E80`, `0xE008`, `0x3108`, `0x1380`), XOR keys (`0xAA`, `0x55`), CRC-8 polynomial (`0x07`).
  - Configure buffer bounds: `GW_MAX_PAYLOAD_SIZE = 65535`, `GW_HEADER_SIZE = 8`, `GW_CRC_SIZE = 1`.

- [ ] **Task 1.3: Platform Abstraction Layer (`include/common/platform.h`, `src/common/platform.c`)**
  - Implement `gw_platform_init()`: call `WSAStartup` on Windows; ignore `SIGPIPE` on Linux.
  - Implement `gw_platform_cleanup()`: call `WSACleanup` on Windows; no-op on Linux.
  - Implement socket creation, option setting (`SO_REUSEADDR`), closing, and non-blocking flags.
  - Implement cross-platform thread creation (`gw_thread_create`) and joining (`gw_thread_join`).

- [ ] **Task 1.4: Structured Logging & Hex Dump Engine (`include/common/logger.h`, `src/common/logger.c`)**
  - Implement log levels: `DEBUG`, `INFO`, `WARN`, `ERROR`.
  - Format log records: `[YYYY-MM-DD HH:MM:SS.mmm] [LEVEL] [MODULE] message`.
  - Implement byte buffer hex dumper: `gw_log_hex_dump(level, tag, buffer, length)` outputting space-separated hex bytes.
  - Support dual output: `stdout` for console and optional file output (`gw_server.log`).

- [ ] **Task 1.5: Unity Test Framework Integration (`tests/unity/`, `tests/test_runner.c`)**
  - Integrate Unity test framework (single header + source).
  - Create test main entry point runnable via `ctest` or direct executable invocation.

### 1.3 Acceptance Criteria
1. CMake successfully generates build files for **MSVC (Windows)**, **MinGW (Windows)**, and **GCC/Clang (Linux)** without errors or warnings.
2. `gw_platform_init()` and `gw_platform_cleanup()` execute cleanly on both OS environments.
3. Socket wrapper creates, binds, and closes a local loopback socket on both Windows and Linux.
4. Thread wrapper successfully spawns and joins 10 parallel worker threads on both platforms.
5. Logger outputs formatted timestamped lines and accurately dumps byte arrays in hex format.
6. `test_runner` compiles and executes with all initial baseline assertions passing (100% pass rate).

### 1.4 Exit Criteria
- [ ] Code passes all compiler checks with warnings treated as errors on both Windows and Linux.
- [ ] Zero static analysis errors reported by `clang-tidy` / MSVC Code Analysis.
- [ ] Baseline test executable runs via `ctest` and exits with code 0.
- [ ] Peer review or architectural sign-off on `gw_types.h` and `platform.h`.

---

## Phase 2 — Protocol Engine & Frame Serialization

### 2.1 Objectives
Develop the frame parsing engine, the 4-stage server validation pipeline, server positive/negative response builders, and client request builders.

### 2.2 Implementation Steps & Tasks

- [ ] **Task 2.1: Header Parsing Engine (`include/common/packet_parser.h`, `src/common/packet_parser.c`)**
  - Parse the 8-byte header from byte buffers using network byte order conversion (`ntohs`, `ntohl`).
  - Extract `source_addr`, `target_addr`, and `length`.
  - Assign zero-copy payload pointer: `pkt->payload = buffer + 8`.
  - Extract CRC byte located at offset `8 + length - 1`.

- [ ] **Task 2.2: Server Validation Pipeline (`include/server/validator.h`, `src/server/validator.c`)**
  - Implement 4 prioritized check stages:
    1. **Length Check (`0x08`)**: Verify `total_bytes >= 9`, `total_bytes == 8 + length`, and `length >= 1`.
    2. **Source Address Check (`0x01`)**: Verify `SA == 0x0E80 || SA == 0xE008`.
    3. **Target Address Check (`0x02`)**: Verify `TA == 0x3108 || TA == 0x1380`.
    4. **Integrity Check (`0x04`)**: Verify `gw_crc8(payload, payload_len) == received_crc`.
  - Immediate fail-fast return: return the first error encountered without evaluating subsequent checks.

- [ ] **Task 2.3: Server Response Builder (`include/server/response_builder.h`, `src/server/response_builder.c`)**
  - **Positive Response**:
    - Swap addresses: response SA = request TA, response TA = request SA.
    - Set response Length = `transformed_payload_len + 1`.
    - Pack encrypted transformed payload.
    - Compute and append CRC-8 over encrypted response payload.
  - **Negative Response**:
    - Fixed length = 2 (`0x00000002`).
    - Embed unencrypted 1-byte `failure_code`.
    - Compute CRC-8 over the single `failure_code` byte and append.

- [ ] **Task 2.4: Client Request Builder (`include/client/request_builder.h`, `src/client/request_builder.c`)**
  - Build valid request frames: encrypt payload with `XOR 0xAA`, compute CRC-8, encode Big-Endian header.
  - Build intentionally invalid request frames: parameterize injection of bad SA, bad TA, bad CRC, and corrupted length for automated negative testing.

- [ ] **Task 2.5: Client Response Parser (`include/client/response_parser.h`, `src/client/response_parser.c`)**
  - Validate response header and CRC-8.
  - Detect response type based on length and payload size:
    - If length == 2: parse unencrypted failure code.
    - If length > 2: decrypt payload using `XOR 0x55` and deliver plain bytes.

- [ ] **Task 2.6: Unit Test Suite (`tests/test_parser.c`, `tests/test_validator.c`)**
  - Test vectors from Section 7 of Problem Statement:
    - Scenario 1 (Valid single byte `0x12`).
    - Scenario 2 (Invalid SA `0xAABB` $\rightarrow$ `0x01`).
    - Scenario 3 (Invalid TA `0x9999` $\rightarrow$ `0x02`).
    - Scenario 4 (Corrupt CRC `0xFF` $\rightarrow$ `0x04`).
    - Scenario 5 (Truncated buffer / bad length $\rightarrow$ `0x08`).

### 2.3 Acceptance Criteria
1. Header parser correctly converts endianness on both Little-Endian architectures (x86/x64) and Big-Endian architectures.
2. All 5 test vectors from the specification produce the exact expected binary bytes.
3. Negative response frames are exactly 10 bytes long, with failure codes unencrypted.
4. Validation precedence is strictly obeyed (e.g., if both Length and SA are bad, `0x08` is returned).
5. All parser and validator unit tests pass on both Linux and Windows.

### 2.4 Exit Criteria
- [ ] 100% of parser and validation unit tests passing in `ctest`.
- [ ] No memory leaks when parsing malformed or boundary-condition packets.
- [ ] Code formatted and documented according to project standards.

---

## Phase 3 — Cryptographic & Transformation Pipeline

### 3.1 Objectives
Implement the CRC-8 algorithm, the XOR encryption/decryption routines, and the high/low nibble swapping engine.

### 3.2 Implementation Steps & Tasks

- [ ] **Task 3.1: CRC-8 Engine Implementation (`include/common/crc8.h`, `src/common/crc8.c`)**
  - Implement bitwise CRC-8 calculation with configurable polynomial (`0x07`) and initial value (`0x00`).
  - Implement fast lookup-table (LUT) accelerated version `gw_crc8_lut()` for high-throughput packet processing.
  - Provide automated verification verifying that bitwise and LUT implementations produce identical results.

- [ ] **Task 3.2: XOR Cryptographic Routines (`include/common/crypto.h`, `src/common/crypto.c`)**
  - Implement in-place buffer XOR encryption/decryption:
    - `gw_decrypt_request_payload(buf, len)`: XOR with `0xAA`.
    - `gw_encrypt_response_payload(buf, len)`: XOR with `0x55`.
  - Validate self-inverting property: `(data ^ 0xAA) ^ 0xAA == data`.

- [ ] **Task 3.3: Nibble Swap Engine (`include/common/transform.h`, `src/common/transform.c`)**
  - Implement in-place nibble swap:
    `data[i] = ((data[i] & 0x0F) << 4) | ((data[i] & 0xF0) >> 4);`
  - Ensure compatibility with full 8-bit range `0x00` through `0xFF`.

- [ ] **Task 3.4: Cryptography & Transform Unit Tests (`tests/test_crypto.c`, `tests/test_crc8.c`, `tests/test_transform.c`)**
  - Test CRC-8 against known test vectors:
    - Empty buffer $\rightarrow$ `0x00`.
    - Single byte `0xB8` $\rightarrow$ `0x54`.
    - Failure code byte `0x01` $\rightarrow$ `0x07`.
    - Multi-byte test sequences.
  - Test nibble swap for edge values (`0x00`, `0xFF`, `0x12` $\rightarrow$ `0x21`, `0x3A` $\rightarrow$ `0xA3`).
  - Verify complete transformation pipeline: `0x12` $\rightarrow$ `0xB8` $\rightarrow$ `0x12` $\rightarrow$ `0x21` $\rightarrow$ `0x74` $\rightarrow$ `0x21`.

### 3.3 Acceptance Criteria
1. CRC-8 produces bit-exact values matching problem statement specifications.
2. In-place XOR and nibble swap execute without buffer overruns or off-by-one errors.
3. Cryptographic unit tests achieve 100% branch coverage.
4. Lookup table initialization is thread-safe and re-entrant.

### 3.4 Exit Criteria
- [ ] All cryptographic unit tests pass on both Windows (MSVC/MinGW) and Linux (GCC/Clang).
- [ ] Performance benchmark of CRC-8 LUT achieves $> 100\,\text{MB/s}$ throughput on standard x86_64 hardware.

---

## Phase 4 — Networking Layer (TCP Server & TCP Client)

### 4.1 Objectives
Build the robust TCP network communication layer, implementing shared two-phase stream framing, the multi-threaded Gateway Server listening on port `13400`, and the Diagnostic Client simulator.

### 4.2 Implementation Steps & Tasks

- [ ] **Task 4.1: Two-Phase TCP Stream Framer (`include/common/stream_framer.h`, `src/common/stream_framer.c`)**
  - Implement `gw_recv_exact(sock, buf, len)`: loop on `recv()` / `recv()` until all requested bytes are gathered or socket closes.
  - Implement `gw_read_frame(sock, buf, max_len, &total_read)`:
    - **Step 1**: Read exactly 8 bytes (Header).
    - **Step 2**: Extract `length` field via `ntohl`.
    - **Step 3**: Validate `length <= GW_MAX_PAYLOAD_SIZE + 1` to prevent memory exhaustion attacks.
    - **Step 4**: Read exactly `length` bytes (Payload + CRC).
  - Handle TCP fragmentation (partial reads) and concatenated frames (multiple frames in one stream).

- [ ] **Task 4.2: Gateway Server Socket Lifecycle (`include/server/tcp_server.h`, `src/server/tcp_server.c`)**
  - Create socket: `AF_INET`, `SOCK_STREAM`, protocol 0.
  - Set socket options: `SO_REUSEADDR`, TCP keepalive.
  - Bind to `0.0.0.0:13400` (or configured interface/port).
  - Listen with backlog of 32.
  - Main loop: accept incoming client sockets, spawn worker thread via `gw_thread_create()`.
  - Support graceful termination on `SIGINT` / `SIGTERM` (Linux) and `SetConsoleCtrlHandler` (Windows).

- [ ] **Task 4.3: Per-Connection Worker Handler (`include/server/conn_handler.h`, `src/server/conn_handler.c`)**
  - Dedicated worker thread function per connected client.
  - Connection loop:
    1. Read full incoming packet via `gw_read_frame()`.
    2. Parse header $\rightarrow$ run `gw_validate_packet()`.
    3. If validation fails $\rightarrow$ call `gw_build_negative_response()` $\rightarrow$ send 10 bytes back.
    4. If validation succeeds $\rightarrow$ decrypt payload (`0xAA`) $\rightarrow$ swap nibbles $\rightarrow$ encrypt (`0x55`) $\rightarrow$ calculate CRC $\rightarrow$ send Positive Response.
    5. Repeat until client disconnects or error occurs.
  - Safely close socket and free thread resources on exit.

- [ ] **Task 4.4: TCP Diagnostic Client Engine (`include/client/tcp_client.h`, `src/client/tcp_client.c`)**
  - Implement `gw_client_connect(client, ip, port)`: resolve address, establish TCP connection.
  - Implement `gw_client_send_request(client, req_bytes, req_len, resp_out)`:
    - Send request frame over socket.
    - Read response frame using `gw_read_frame()`.
    - Parse response into `gw_client_response_t` (type, addresses, failure code, or decrypted payload).
  - Implement `gw_client_disconnect(client)`.

- [ ] **Task 4.5: Network Layer Mock & Loopback Tests (`tests/test_stream_framer.c`)**
  - Test stream framer handling fragmented deliveries (feed byte-by-byte into socket pair/loopback).
  - Test stream framer handling multiple requests in a single TCP payload.

### 4.3 Acceptance Criteria
1. Server binds and listens on port `13400` on both Linux and Windows.
2. Stream framer cleanly reconstructs frames split across arbitrary 1-byte, 2-byte, or multi-byte TCP chunks.
3. Worker thread correctly processes pipelined requests over a single persistent TCP connection.
4. Server gracefully handles client abrupt disconnects without crashing or leaking socket descriptors.
5. Signal handler cleanly stops accept loop and releases listening socket on both platforms.

### 4.4 Exit Criteria
- [ ] Successful loopback connection test between client and server on both Windows and Linux.
- [ ] No socket descriptor leaks verified via OS diagnostic tools (`netstat`, `lsof`).
- [ ] Zero unhandled socket error exceptions.

---

## Phase 5 — Integration, Logging & Observability

### 5.1 Objectives
Integrate Server and Client binaries, add structured real-time tracing with hex dumping, implement atomic telemetry counters, and create user-friendly CLI interfaces for both applications.

### 5.2 Implementation Steps & Tasks

- [ ] **Task 5.1: Gateway Server Executable (`src/server/server_main.c`)**
  - Command-line argument parsing (`--port`, `--ip`, `--log-file`, `--verbose`).
  - Signal handling setup for clean shutdown (`Ctrl+C`).
  - Startup banner displaying version, listening port, and active platform.
  - Telemetry summary printing upon shutdown.

- [ ] **Task 5.2: Diagnostic Client Executable (`src/client/client_main.c`)**
  - Implement two operating modes:
    1. **Interactive CLI Mode (`--interactive`)**:
       - Command prompt allowing user to specify SA, TA, and raw payload hex string.
       - Displays outgoing frame hex dump.
       - Displays incoming response, status (Positive vs Negative), failure reason, and decrypted payload bytes.
    2. **Automated Conformance Test Mode (`--test`)**:
       - Runs all 5 standard specification test scenarios against target server.
       - Prints PASS/FAIL summary table and returns exit code 0 (all pass) or 1 (failures).

- [ ] **Task 5.3: Server Telemetry & Metrics (`include/server/server_metrics.h`, `src/server/server_metrics.c`)**
  - Implement atomic counters (C11 `<stdatomic.h>` / Win32 `InterlockedIncrement`):
    - `total_connections`
    - `total_requests_received`
    - `positive_responses_sent`
    - `negative_responses_sent`
    - Individual failure counters: `fail_0x01_sa`, `fail_0x02_ta`, `fail_0x04_crc`, `fail_0x08_len`.
  - Expose `gw_metrics_dump()` invoked on server shutdown.

- [ ] **Task 5.4: Live Hex Tracing Integration**
  - Log complete hex strings of received request frames (`RX`) and transmitted response frames (`TX`).
  - Trace internal payload decryption, transformation, and re-encryption states at `DEBUG` log level.

### 5.3 Acceptance Criteria
1. Server executable launches from command prompt/terminal on both Windows (`gw_server.exe`) and Linux (`./gw_server`).
2. Client executable (`gw_client`) connects to `gw_server`, executes the automated test suite, and reports 100% PASS on both platforms.
3. Interactive client mode accurately sends custom payloads and displays decrypted response data.
4. Server prints structured logs showing connection events, packet traces, and validation results.
5. Metrics counters accurately reconcile: `requests == positive_responses + negative_responses`.

### 5.4 Exit Criteria
- [ ] Both executables build cleanly in Release and Debug modes.
- [ ] End-to-end integration test passes in continuous execution for 10 minutes without error.
- [ ] Clean shutdown verified with metrics dumped to console.

---

## Phase 6 — Testing, Hardening & Dual-Platform Verification

### 6.1 Objectives
Conduct exhaustive system testing, stress testing under concurrent load, fuzzing for resilience against malformed inputs, memory sanitization, and packaging.

### 6.2 Implementation Steps & Tasks

- [ ] **Task 6.1: End-to-End Integration Test Suite (`tests/test_integration.c`)**
  - Automated integration test suite launching server on loopback, running client requests, and asserting response bytes:
    - Test Case 1: Valid 1-byte payload (`0x12` $\rightarrow$ `0x21`).
    - Test Case 2: Valid multi-byte payload (`0x01 0x02 0x03 0x04`).
    - Test Case 3: Invalid SA `0x1122` $\rightarrow$ Negative Response `0x01`.
    - Test Case 4: Invalid TA `0x3344` $\rightarrow$ Negative Response `0x02`.
    - Test Case 5: Corrupted CRC $\rightarrow$ Negative Response `0x04`.
    - Test Case 6: Payload length mismatch $\rightarrow$ Negative Response `0x08`.
    - Test Case 7: Alternating valid and invalid requests on single persistent connection.

- [ ] **Task 6.2: Multi-Client Concurrency & Zero-Drop Stress Test (`tests/test_concurrent.c`)**
  - Spawn 32 concurrent client threads connecting simultaneously.
  - Each client sends 500 requests (mix of valid and invalid).
  - Verify server's **Zero-Drop Policy**: total responses received across all clients must exactly equal total requests sent ($32 \times 500 = 16,000$ requests).
  - Verify no deadlocks, socket exhaustion, or race conditions.

- [ ] **Task 6.3: Fuzzing & Malformed Packet Hardening (`tests/test_fuzz.c`)**
  - Inject random garbage bytes, partial headers (1 to 7 bytes followed by disconnect), extreme length values (`0xFFFFFFFF`), and corrupted CRC bytes.
  - Assert that server does not crash, hang, leak memory, or throw unhandled exceptions.

- [ ] **Task 6.4: Memory Sanitization & Leak Checks**
  - **Linux**: Run full test suite under Valgrind (`valgrind --leak-check=full --error-exitcode=1`) and Clang AddressSanitizer (`-fsanitize=address,undefined`).
  - **Windows**: Build and run with MSVC AddressSanitizer (`/fsanitize=address`) and verify CRT debug heap report.
  - Target: **Zero memory leaks, zero buffer overruns, zero use-after-free**.

- [ ] **Task 6.5: Performance Benchmarking**
  - Measure per-packet processing latency (excluding network wire time).
  - Assert processing latency is $< 1.0\,\text{ms}$ per packet.

- [ ] **Task 6.6: Documentation & Packaging**
  - Write user guide in `README.md` with build and run instructions for Windows and Linux.
  - Create packaging scripts (`build.bat` for Windows, `build.sh` for Linux).

### 6.3 Acceptance Criteria
1. Integration test suite passes 100% of functional tests on both Windows and Linux.
2. Concurrent stress test processes 16,000 requests without a single dropped or skipped request.
3. Fuzzing test runs 10,000 malformed packets with 0 server crashes.
4. Valgrind and AddressSanitizer report zero memory leaks and zero memory corruption errors.
5. Average packet processing latency is $< 100\,\mu\text{s}$ (well within the $1\,\text{ms}$ target).

### 6.4 Exit Criteria
- [ ] All test targets pass via `ctest` on Windows (MSVC) and Linux (GCC).
- [ ] Clean reports from AddressSanitizer on both platforms.
- [ ] Project documentation and build scripts verified by executing clean builds on fresh environments.
- [ ] Final release readiness checklist signed off.

---

## 7. Dual-Platform Build & Verification Guide

### 7.1 Linux Build & Test Commands (GCC / Clang)

```bash
# 1. Configure CMake with AddressSanitizer
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON

# 2. Build all targets (common lib, server, client, tests)
cmake --build build --config Debug -j$(nproc)

# 3. Run all unit and integration tests
ctest --test-dir build --output-on-failure

# 4. Run Memory Leak Check with Valgrind
valgrind --leak-check=full --show-leak-kinds=all --error-exitcode=1 ./build/gw_tests

# 5. Start Gateway Server
./build/gw_server --port 13400 --verbose

# 6. Run Diagnostic Client in Test Mode (in separate terminal)
./build/gw_client --ip 127.0.0.1 --port 13400 --test
```

### 7.2 Windows Build & Test Commands (MSVC / PowerShell)

```powershell
# 1. Configure CMake for Visual Studio 2022
cmake -B build -S . -G "Visual Studio 17 2022" -A x64

# 2. Build all targets in Release configuration
cmake --build build --config Release -j

# 3. Run all tests via CTest
ctest --test-dir build -C Release --output-on-failure

# 4. Start Gateway Server
.\build\Release\gw_server.exe --port 13400 --verbose

# 5. Run Diagnostic Client in Test Mode (in separate PowerShell window)
.\build\Release\gw_client.exe --ip 127.0.0.1 --port 13400 --test

# 6. Run Diagnostic Client in Interactive Mode
.\build\Release\gw_client.exe --ip 127.0.0.1 --port 13400 --interactive
```

---

## 8. Master Phase Deliverables & Traceability Matrix

| Phase | Core Deliverables | Problem Statement Requirement Mapped | Primary Verification Method |
| :---: | :--- | :--- | :--- |
| **Phase 1** | `CMakeLists.txt`, `gw_types.h`, `platform.h/.c`, `logger.h/.c`, Unity | Dual-platform foundation, C11 standard | Cross-compilation, platform socket/thread tests |
| **Phase 2** | `packet_parser.c`, `validator.c`, `response_builder.c`, `request_builder.c` | Failure codes `0x01`, `0x02`, `0x04`, `0x08`, priority order | Unit tests matching 5 specification test vectors |
| **Phase 3** | `crc8.c`, `crypto.c`, `transform.c` | XOR `0xAA`, XOR `0x55`, Nibble Swap, CRC-8 (`0x07`) | Math & bit-trace unit tests, CRC-8 LUT benchmark |
| **Phase 4** | `stream_framer.c`, `tcp_server.c`, `conn_handler.c`, `tcp_client.c` | TCP port `13400`, concurrent multi-requests, zero-drop | Loopback streaming & fragmented packet tests |
| **Phase 5** | `server_main.c`, `client_main.c`, `server_metrics.c` | Client/Server interaction, observability, logging | End-to-end interactive CLI & automated test mode |
| **Phase 6** | `test_integration.c`, `test_concurrent.c`, `test_fuzz.c`, `README.md` | Non-functional: $<1\text{ms}$, crash resistance, zero leaks | Valgrind, AddressSanitizer, 16k request stress test |
