#!/bin/bash
# ==============================================================================
# Gateway (GW) Network Protocol - Multi-Client Concurrent Stress & Loss Checker
# ==============================================================================

NUM_CLIENTS=${1:-5}
PACKETS_PER_CLIENT=${2:-50}
PORT=${3:-13400}
SERVER_IP=${4:-"127.0.0.1"}

# Detect executable extension (Windows Git Bash vs Linux)
SERVER_BIN="./gw_server"
CLIENT_BIN="./gw_client"
if [ -f "./gw_server.exe" ]; then
    SERVER_BIN="./gw_server.exe"
fi
if [ -f "./gw_client.exe" ]; then
    CLIENT_BIN="./gw_client.exe"
fi

echo "============================================================"
echo " Gateway Multi-Client Packet Transfer & Loss Test"
echo "   Target:               $SERVER_IP:$PORT"
echo "   Simultaneous Clients: $NUM_CLIENTS"
echo "   Packets per Client:   $PACKETS_PER_CLIENT"
echo "   Total Packets:        $((NUM_CLIENTS * PACKETS_PER_CLIENT))"
echo "============================================================"
echo ""

# Check if server is already running on the port
SERVER_PID=""
SERVER_LOG="server_multi.log"
rm -f "$SERVER_LOG"

# Try connecting to port to see if server already exists
if ! (exec 3<>/dev/tcp/$SERVER_IP/$PORT) 2>/dev/null; then
    echo "[SETUP] Launching Gateway Server on port $PORT..."
    $SERVER_BIN --port "$PORT" --log-file "$SERVER_LOG" &
    SERVER_PID=$!

    # Wait for server to bind and accept connections
    for attempt in $(seq 1 20); do
        sleep 0.1
        if (exec 3<>/dev/tcp/$SERVER_IP/$PORT) 2>/dev/null; then
            exec 3<&-
            exec 3>&-
            break
        fi
    done
    echo "[SETUP] Gateway Server is ready and listening on port $PORT."
else
    exec 3<&- 2>/dev/null || true
    exec 3>&- 2>/dev/null || true
    echo "[SETUP] Gateway Server is already active on port $PORT."
fi

# Run clients in background simultaneously
echo "[EXEC] Spawning $NUM_CLIENTS simultaneous client processes..."
TMP_LOG=$(mktemp)
CLIENT_PIDS=()

START_TIME=$(date +%s%N 2>/dev/null || date +%s)

for i in $(seq 1 "$NUM_CLIENTS"); do
    $CLIENT_BIN --ip "$SERVER_IP" --port "$PORT" --client-id "$i" --count "$PACKETS_PER_CLIENT" >> "$TMP_LOG" 2>&1 &
    CLIENT_PIDS+=($!)
done

# IMPORTANT: Wait ONLY for the client processes, NOT for the server daemon!
wait "${CLIENT_PIDS[@]}"

END_TIME=$(date +%s%N 2>/dev/null || date +%s)

echo ""
echo "--- Client Execution Logs ---"
cat "$TMP_LOG"

# Parse total sent, received, and lost packets from client outputs
TOTAL_SENT=0
TOTAL_RECV=0
TOTAL_LOST=0

while IFS= read -r line; do
    if [[ "$line" =~ Sent=([0-9]+)\ \|\ Received=([0-9]+)\ \|\ Lost=([0-9]+) ]]; then
        TOTAL_SENT=$((TOTAL_SENT + ${BASH_REMATCH[1]}))
        TOTAL_RECV=$((TOTAL_RECV + ${BASH_REMATCH[2]}))
        TOTAL_LOST=$((TOTAL_LOST + ${BASH_REMATCH[3]}))
    fi
done < "$TMP_LOG"

# Stop server if we started it
if [ -n "$SERVER_PID" ]; then
    echo ""
    echo "[CLEANUP] Stopping Gateway Server (PID: $SERVER_PID)..."
    kill -2 "$SERVER_PID" 2>/dev/null || kill "$SERVER_PID" 2>/dev/null || true
    wait "$SERVER_PID" 2>/dev/null || true
fi

rm -f "$TMP_LOG"

echo ""
echo "============================================================"
echo " Multi-Client Test Results & Loss Verification"
echo "============================================================"
echo "  Concurrent Clients:      $NUM_CLIENTS"
echo "  Total Packets Sent:      $TOTAL_SENT"
echo "  Total Packets Received:  $TOTAL_RECV"
if [ "$TOTAL_LOST" -eq 0 ]; then
    echo "  Total Packets Lost:      $TOTAL_LOST (0.00% packet loss - ZERO-DROP OK)"
else
    echo "  Total Packets Lost:      $TOTAL_LOST (PACKET LOSS DETECTED)"
fi
if [ -f "$SERVER_LOG" ]; then
    echo "  Server Log File:         $SERVER_LOG"
fi
echo "============================================================"
echo ""
