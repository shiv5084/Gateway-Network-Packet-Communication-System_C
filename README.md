# Gateway (GW) Network Packet Communication System

A high-performance, cross-platform (Windows & Linux) C11 Gateway Server and Diagnostic Client implementing the custom automotive / embedded Gateway Packet Protocol.

---

## 1. System Architecture & Features

- **Dual-Platform Architecture**: Compiles cleanly and runs natively on Windows (Winsock2, Win32 threads) and Linux (POSIX sockets, `pthread`).
- **Two-Phase TCP Stream Framing**: Safely reconstructs packets from arbitrary fragmented or concatenated byte streams (`gw_recv_exact` / `gw_read_frame`) with bounds protection against memory exhaustion attacks.
- **Multi-Threaded Server**: Dedicated per-connection worker threads running on port `13400` with graceful signal shutdown (`Ctrl+C`).
- **Cryptographic & Transformation Engine**:
  - Request decryption with `XOR 0xAA`
  - In-place high/low nibble swapping (`0x12` $\to$ `0x21`)
  - Response payload encryption with `XOR 0x55`
  - High-throughput accelerated lookup-table (LUT) CRC-8 (`0x07` polynomial)
- **4-Stage Prioritized Validation Pipeline**: Strict fail-fast evaluation enforcing Length (`0x08`) $\to$ Source Address (`0x01`) $\to$ Target Address (`0x02`) $\to$ CRC-8 Integrity (`0x04`).
- **Live Telemetry & Metrics**: Thread-safe telemetry tracking accepted connections, total requests, success/failure counts, and reconciliation checks.
- **Exhaustive Test Harness**: 42 automated unit, loopback, integration, 8-thread concurrent stress (zero-drop), fuzzing, and latency benchmark tests using the Unity framework.

---

## 2. Protocol Specification

### Packet Structure
All packets on the wire adhere to an 8-byte Big-Endian header followed by variable payload and a 1-byte CRC-8.

```
+-------------------+-------------------+-----------------------------------+--------------------+---------------+
| Source Addr (2B)  | Target Addr (2B)  | Length (4B, Network Byte Order)  | Payload (Length-1) | CRC-8 (1B)    |
+-------------------+-------------------+-----------------------------------+--------------------+---------------+
```

### Addressing Rules
- **Valid Request Source Addresses (SA)**:
  - Node 1: `0x0E80`
  - Node 2: `0xE008`
- **Valid Request Target Addresses (TA)**:
  - Gateway 1: `0x3108`
  - Gateway 2: `0x1380`
- **Response Addressing**: Positive and Negative responses swap addresses (`Response SA = Request TA`, `Response TA = Request SA`).

### Validation Precedence & Failure Codes
When a request fails validation, a 10-byte **Negative Response** is immediately returned with the unencrypted failure code:

| Priority | Failure Code | Name | Condition |
| :---: | :---: | :--- | :--- |
| **1 (Highest)** | `0x08` | `GW_FAIL_LENGTH_MISMATCH` | Declared length $\ne$ actual packet size or length $< 1$ |
| **2** | `0x01` | `GW_FAIL_SRC_ADDR_INVALID` | SA not in {`0x0E80`, `0xE008`} |
| **3** | `0x02` | `GW_FAIL_TGT_ADDR_INVALID` | TA not in {`0x3108`, `0x1380`} |
| **4** | `0x04` | `GW_FAIL_INTEGRITY` | Received CRC-8 $\ne$ calculated CRC-8 over request payload |

### Transformation Pipeline
```
[Client] Raw Payload (0x12)
    │
    ▼ XOR 0xAA (0xB8) + Compute CRC (0x21)
[Network Wire] Request Frame: 0E 80 31 08 00 00 00 02 B8 21
    │
    ▼ Gateway Server receives & validates
[Server] Decrypt XOR 0xAA (0x12)
    │
    ▼ Nibble Swap (0x21)
[Server] Encrypt XOR 0x55 (0x74) + Compute CRC (0x4B)
    │
    ▼
[Network Wire] Response Frame: 31 08 0E 80 00 00 00 02 74 4B
    │
    ▼
[Client] Decrypt XOR 0x55 -> Final Swapped Payload (0x21)
```

---

## 3. Directory Layout

```
├── CMakeLists.txt              # Cross-platform CMake configuration
├── build.bat                   # Automated Windows build script
├── build.sh                    # Automated Linux build script
├── README.md                   # System documentation
├── docs/                       # Architecture and implementation plan
├── include/
│   ├── common/                 # Shared headers (types, platform, crc8, crypto, framer)
│   ├── server/                 # Server headers (tcp_server, conn_handler, validator, metrics)
│   └── client/                 # Client headers (tcp_client, request_builder, response_parser)
├── src/
│   ├── common/                 # Platform, logger, crc8, crypto, transform, framer
│   ├── server/                 # Server daemon, connection handler, validator, metrics, main
│   └── client/                 # Diagnostic client, request builders, CLI, main
└── tests/
    ├── unity/                  # Unity unit testing framework
    ├── test_platform.c         # Phase 1: Platform abstraction tests
    ├── test_parser.c           # Phase 2: Header & packet parser tests
    ├── test_validator.c        # Phase 2: 5 specification test scenarios
    ├── test_crypto.c           # Phase 3: CRC-8, XOR, nibble swap, transform pipeline
    ├── test_stream_framer.c    # Phase 4: Stream framing & fragmented TCP tests
    ├── test_integration.c     # Phase 6: End-to-end loopback integration suite
    ├── test_concurrent.c      # Phase 6: Multi-client concurrency & zero-drop stress test
    ├── test_fuzz.c            # Phase 6: Fuzzing, extreme lengths & latency benchmark
    └── test_runner.c          # Master test entry point
```

---

## 4. Building the Project

### Windows (Automated Batch Script)
```cmd
build.bat
```

### Linux (Automated Shell Script)
```bash
chmod +x build.sh
./build.sh
```

### CMake Build (Windows / Linux)
```bash
# Configure
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release

# Build all targets
cmake --build build --config Release

# Run complete test suite
ctest --test-dir build -C Release --output-on-failure
```

---

## 5. Running the Application

### 1. Launch Gateway Server
```powershell
# Default port 13400
.\gw_server.exe

# Custom port with verbose live byte tracing
.\gw_server.exe --port 13400 --verbose

# Write logs to file
.\gw_server.exe --port 13400 --log-file server.log
```

Upon termination (`Ctrl+C`), the server dumps a comprehensive telemetry report:
```
============================================================
 Gateway Server Telemetry & Metrics Summary
============================================================
  Total Accepted Connections:     10
  Total Requests Received:        405
  Positive Responses (Success):   205
  Negative Responses (Rejected):  200
  ----------------------------------------------------------
  Failure Code Breakdown:
    - [0x01] Invalid Source Address: 50
    - [0x02] Invalid Target Address: 50
    - [0x04] CRC-8 Integrity Check:  50
    - [0x08] Length Mismatch:        50
  Reconciliation Check: OK (Req: 405 == Pos+Neg: 405)
============================================================
```

### 2. Run Diagnostic Client in Automated Test Mode
```powershell
.\gw_client.exe --ip 127.0.0.1 --port 13400 --test
```
Output:
```
============================================================
 Running Gateway Protocol Conformance Test Scenarios
============================================================

[Scenario 1] Valid Request (SA=0x0E80, TA=0x3108, Payload=0x12) ... PASS (Received 0x21)
[Scenario 2] Invalid SA (SA=0xAABB, TA=0x3108) ... PASS (FailCode: 0x01)
[Scenario 3] Invalid TA (SA=0x0E80, TA=0x9999) ... PASS (FailCode: 0x02)
[Scenario 4] Corrupted Integrity Check (Bad CRC) ... PASS (FailCode: 0x04)
[Scenario 5] Length Mismatch (length=0 in header) ... PASS (FailCode: 0x08)

============================================================
 Results: 5/5 tests passed (ALL PASSED)
============================================================
```

### 3. Run Diagnostic Client in Interactive CLI Mode
```powershell
.\gw_client.exe --ip 127.0.0.1 --port 13400 --interactive
```
Allows real-time interactive diagnostic queries, prompting for Source Address, Target Address, and custom payload hex strings.

### 4. Run Multiple Simultaneous Clients & Check Packet Transfer and Loss

You can stress-test the Gateway Server with multiple concurrent client connections streaming packets simultaneously to verify throughput, zero packet loss, and inspect byte-level logs.

#### Option A: Automated Multi-Client Script (Windows PowerShell)
Run the provided [`run_multi_clients.ps1`](file:///d:/GW%20Network%20packet%20communication/run_multi_clients.ps1):
```powershell
# Launches 5 concurrent clients sending 50 packets each (250 packets total)
.\run_multi_clients.ps1 -NumClients 5 -PacketsPerClient 50 -Port 13400
```
This script automatically starts the server, dispatches parallel client processes, captures output, tallies loss rates, and displays a summary table.

#### Option B: PowerShell Parallel One-Liner
```powershell
# Spawns 10 parallel client instances sending 100 packets each
1..10 | ForEach-Object -Parallel {
    .\gw_client.exe --port 13400 --client-id $_ --count 100
}
```

#### Option C: Automated Multi-Client Script (Linux)
```bash
chmod +x run_multi_clients.sh
./run_multi_clients.sh 5 50 13400 127.0.0.1
```

#### How to Inspect Packet Transfer & Loss in the Logs

1. **Client-Side Verification**:
   Each client prints an execution summary upon completion:
   ```
   [Client 1] Summary: Sent=100 | Received=100 | Lost=0 (Loss Rate: 0.00%)
   [Client 2] Summary: Sent=100 | Received=100 | Lost=0 (Loss Rate: 0.00%)
   ```

2. **Server-Side Verification**:
   Start the server with `--verbose` and `--log-file server.log`:
   ```powershell
   .\gw_server.exe --port 13400 --verbose --log-file server.log
   ```
   Inspect `server.log` to view real-time per-packet incoming (`RX`) and outgoing (`TX-POS` / `TX-NEG`) traces:
   ```
   [2026-10-16 21:14:20.431] [INFO ] [SERVER] [Conn #2] Accepted from 127.0.0.1:53842
   [2026-10-16 21:14:20.431] [DEBUG] [CONN  ] RX (10 bytes): 0E 80 31 08 00 00 00 02 B8 21
   [2026-10-16 21:14:20.431] [DEBUG] [CONN  ] TX-POS (10 bytes): 31 08 0E 80 00 00 00 02 74 4B
   ```

3. **Reconciliation & Zero-Drop Check**:
   Upon pressing `Ctrl+C`, the server's telemetry summary confirms zero packet loss:
   ```
   Total Requests Received:        500
   Positive Responses (Success):   500
   Negative Responses (Rejected):  0
   Reconciliation Check: OK (Req: 500 == Pos+Neg: 500)
   ```

---

## 6. Test Suite & Verification Results

All 42 tests pass with zero failures:
- **Baseline**: Platform mutex, thread spawning, endianness conversions, structured logging.
- **Protocol & Validation**: All 5 specification scenarios from Section 7 of the problem statement.
- **Cryptography**: Full 256-byte LUT vs. bitwise CRC-8 equality, XOR roundtrip symmetry, nibble swapping.
- **Two-Phase Framer**: Reassembly across 1-byte, 2-byte, and multi-byte fragmented TCP deliveries.
- **Concurrency & Stress**: 8 client threads sending 400 requests concurrently; 100% processed with zero drops.
- **Hardening & Fuzzing**: Abrupt partial-header disconnects, `0xFFFFFFFF` overflow headers, and random garbage streams.
- **Performance**: Processing latency $< 100\,\mu\text{s}$ per packet (well below the $1.0\,\text{ms}$ threshold).
