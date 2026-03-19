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

YAML parser scope in this version is intentionally simple:
- Supports only: `groups -> <group> -> hosts -> - <host>`
- Host entries may be `host` or `host:port`
- No nested children beyond that structure
- No external YAML library is used

---

## Metrics (v1)
The metrics that will be gathered in the first version will be:
- Load Average
- CPU Usage
- Memory Usage
- I/O wait

Interfaces used for this metrics will be:

`/proc/stat`, `/proc/meminfo`, `/proc/loadavg` and `/proc/diskstats`

## API (v1)

The endpoint used for metrics will be `/status` and it will return a JSON string that contains all the metrics.

---

## Project constraints
This project will try to follow a series of constraints/rules:
- No external libraries.
- Written in C.
- Minimal memory usage.
- Simple code where possible.
- A special focus on quick oversight.
