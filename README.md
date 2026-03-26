# Nodestatus project

## Project idea
 A simple and lightweight tool to obtain hardware metrics from groups of nodes.

## Quick architecture description

- CLI tool gets system status from every node via an HTTP endpoint exposed by the agent.
- Node Agent reads system data directly from standard Linux-kernel interfaces. (`/sys`, `/proc`).

                    +------------------------+
                    |      User / Admin      |
                    |  (run `nodectl status`)|
                    +-----------+------------+
                                |
                                | CLI-command
                                v
                        +-------+--------+
                        |    CLI-tool    |
                        |   `nodectl`    |
                        +-------+--------+
                                |
                                | HTTP GET /status
                                | (parallel requests)
        ---------------------------------------------------------------
        |                          Nodes                               |
        |                                                              |
        |   +-------------------+    +-------------------+             |
        |   |    Node Agent     |    |    Node Agent     |   ...       |
        |   |  (port 9002)      |    |  (port 9002)      |             |
        |   |  /status endpoint |    |  /status endpoint |             |
        |   +---------+---------+    +---------+---------+             |
        |             |                       |                         |
        |   reads from|                       |reads from               |
        |   /proc, /sys                      /proc, /sys                |
        ---------------------------------------------------------------

---
## Agent's responsibilites
The agent takes care of:
- Listening on port `9002` by default.
- Managing the `/status` endpoint where metrics are exposed.
- Reading and parsing system metrics from the standard Linux interfaces.
- Returning the JSON with the metrics.

You can start the agent on a custom port:
- `./bin/agent` (default: `9002`)
- `./bin/agent --port 9010`
- `./bin/agent -p 9010`

## CLI tool responsibilities
The CLI tool takes care of:
- Reading the list of nodes.
- Send parallel requests to the agent's HTTP endpoint.
- Obtain the results.
- Show the table.

For example, the command `nodectl status` will output a table like this one:

| HOST   | CPU | MEM | LOAD |
| ------ | --- | --- | ---- |
| node01 | 23% | 14% | 0.3  |
| node02 | 87% | 84% | 4.1  |
| node03 | 12% | 35% | 0.2  |

### Node groups (YAML inventory)

Nodes are grouped in `config/inventory.yaml`:

```yaml
groups:
  HPC:
    hosts:
      - host1
      - host2:9010
  LAB:
    hosts:
      - host3
```

Per-node custom ports are supported directly in inventory with `host:port`.
If no port is provided, CLI uses default agent port `9002`.

Commands:
- `nodectl status` → all configured nodes
- `nodectl status HPC` → only nodes in group `HPC`
- `nodectl watch` → continuous refresh for all nodes
- `nodectl watch HPC` → continuous refresh for group `HPC`
- `nodectl status --sort host|resp|state` → choose table sort mode (default: `state`)
- `nodectl watch --sort host|resp|state` → keep watch sorted by selected mode
- `nodectl status --no-color` → disable ANSI color cues


Options:
- `--sort host|resp|state`: Sort nodes by hostname, response time (default: state-health)
- `--no-color`: Disable ANSI color codes
- `--filter <expr>`: Filter nodes by criteria (e.g., `state=OK`, `cpu>80`, `mem<50`)

**Drill-Down Views** (for resource-specific troubleshooting):
- `--cpu`: Show detailed CPU breakdown (user%, nice%, sys%, idle%, iowait%, busy%)
- `--mem`: Show detailed memory info (total, available, mem%, swap total, swap used, swap%)
- `--disk`: Show detailed disk I/O (read/write MB/s, read/write IOPS, total IOPS)
- `--net`: Show detailed network stats (RX/TX MB/s, total bandwidth)
- `--debug`: Show raw diagnostic output (HOST, STATE, RESP, AGE, BYTES, TS, FETCH status)

**Filtering** (narrow down displayed nodes):
- `--filter 'state=OK'` - Show only healthy nodes
- `--filter 'state!=OK'` - Show only failed nodes
- `--filter 'cpu>80'` - Show high CPU usage nodes
- `--filter 'mem>90'` - Show high memory usage nodes
- `--filter 'load>=2.0'` - Show nodes with high load average
- `--filter 'resp<10'` - Show nodes with fast response times
- Multiple filters use AND logic: `--filter 'state=OK' --filter 'cpu>50'`

Supported filter fields: `state`, `cpu`, `mem`, `load`, `disk`, `resp`
Supported operators: `=`, `!=`, `>`, `<`, `>=`, `<=`

Usage examples:
```bash
# Default overview (simple, fast)
./bin/cli status

# Sort by response time and disable colors
./bin/cli status --sort resp --no-color

# Watch mode with auto-refresh
./bin/cli watch

# Drill down into CPU metrics for troubleshooting
./bin/cli status --cpu

# Drill down into memory metrics (includes swap)
./bin/cli status --mem

# Check disk I/O and IOPS
./bin/cli status --disk

# Monitor network bandwidth
./bin/cli status --net

# Debug view - raw diagnostics for troubleshooting
./bin/cli status --debug

# Filter to show only healthy nodes
./bin/cli status --filter 'state=OK'

# Filter high CPU nodes and sort by CPU
./bin/cli status --filter 'cpu>80' --sort cpu

# Combine filtering with drill-down views
./bin/cli status --cpu --filter 'state=OK' --filter 'cpu>50'

# Watch mode with filters for real-time monitoring
./bin/cli watch --filter 'state!=OK'
```

Debug view example output:
```
HOST: localhost:9002
STATE: OK
RESP: 2ms
AGE: 5ms
BYTES: 512
TS: 1710879401123
FETCH:
  connect: ok
  read: ok
```

YAML parser scope in this version is intentionally simple:
- Supports only: `groups -> <group> -> hosts -> - <host>`
- Host entries may be `host` or `host:port`
- No nested children beyond that structure
- No external YAML library is used

---

## Metrics (current version)
The agent collects comprehensive system metrics:

**CPU Metrics:**
- User%, Nice%, System%, Idle%, I/O Wait%, Busy%
- Derived from `/proc/stat`

**Memory Metrics:**
- Total, Free, Available, Used (MB and %)
- Swap Total, Free, Used (MB and %)
- Derived from `/proc/meminfo`

**Load Average:**
- 1-minute, 5-minute, 15-minute averages
- Derived from `/proc/loadavg`

**Disk I/O:**
- Read/Write throughput (MB/s)
- Read/Write IOPS
- Total IOPS
- Derived from `/proc/diskstats`

**Network:**
- RX/TX throughput (MB/s)
- Total bandwidth
- Derived from `/sys/class/net/*/statistics/`

## API (current version)

**Endpoint:** `GET /status`

**Response format:** JSON with all metrics

Example response:
```json
{
  "sampleTsMs": 512382,
  "sampleAgeMs": 9,
  "load1": 0.79,
  "load5": 0.71,
  "load15": 0.43,
  "memTotal": 15810400,
  "memFree": 7733048,
  "memAvailable": 11085736,
  "memUsed": 4724664,
  "memUsedPercent": 29.88,
  "swapTotal": 4456440,
  "swapFree": 4456440,
  "swapUsed": 0,
  "swapUsedPercent": 0.00,
  "cpuUser": 29537,
  "cpuNice": 2083,
  "cpuSystem": 11118,
  "cpuIdle": 668027,
  "cpuIOwait": 1403,
  "cpuBusy": 105,
  "cpuBusyPercent": 2.76,
  "diskReadMBps": 0.00,
  "diskWriteMBps": 0.00,
  "diskTotalMBps": 0.00,
  "diskReadIOPS": 0.00,
  "diskWriteIOPS": 0.00,
  "diskTotalIOPS": 0.00,
  "netRxMBps": 0.02,
  "netTxMBps": 0.02,
  "netTotalMBps": 0.04
}
```

---

## Project constraints
This project will try to follow a series of constraints/rules:
- No external libraries.
- Written in C.
- Minimal memory usage.
- Simple code where possible.
- A special focus on quick oversight.
