# Nodestatus - Feature Roadmap

This document outlines potential features and improvements for nodestatus.

## Completed (v0.1)

- [x] Basic agent with CPU, memory, load, disk, network metrics
- [x] CLI with parallel node queries
- [x] YAML inventory with groups and custom ports
- [x] Watch mode with connection keep-alive
- [x] Response time (RESP) tracking
- [x] Sorting modes (host, response, state)
- [x] Color-coded output
- [x] Drill-down views (--cpu, --mem, --disk, --net)
- [x] Debug diagnostic view (--debug)

---

## Proposed Features

### High Priority - Quick Wins

#### 1. Filtering & Search
**Description**: Filter displayed nodes based on criteria.

**Proposed flags**:
- `--filter state=OK` - Show only nodes matching state
- `--filter state=FAILED` - Show only failed nodes
- `--filter cpu>80` - Show nodes with CPU usage above threshold
- `--filter mem>90` - Show nodes with high memory usage
- `--search pattern` - Find nodes by hostname pattern (supports wildcards)

**Use cases**:
- Large clusters where you only want to see problem nodes
- Quickly finding specific hosts
- Focusing on high-resource consumers

**Implementation notes**:
- Add filter parsing in CLI argument handling
- Apply filters after fetch but before sort
- Support multiple filters (AND logic)
- Consider OR logic with `--filter "cpu>80 OR mem>90"`

---

#### 2. Export & Integration
**Description**: Output data in machine-readable formats.

**Proposed flags**:
- `--json` - Output in JSON format
- `--csv` - Output in CSV format
- `--format <type>` - Generic format selector

**Use cases**:
- Integration with monitoring systems
- Log aggregation
- Spreadsheet analysis
- Scripting and automation

**Implementation notes**:
- JSON should output array of node objects
- CSV should have header row
- Maintain same data structure across formats
- Consider `--json-pretty` for human-readable JSON

**Example JSON output**:
```json
[
  {
    "hostname": "localhost:9002",
    "state": "OK",
    "resp_ms": 2,
    "cpu_percent": 45.2,
    "mem_percent": 67.8,
    "load": 1.23,
    "disk_mbps": 12.45
  }
]
```

---

#### 3. Thresholds & Alerts
**Description**: Highlight nodes exceeding thresholds and provide alerting.

**Proposed features**:
- Visual highlighting of threshold violations
- `--threshold cpu=80,mem=90,load=4.0` - Custom warning levels
- Exit codes based on health state:
  - 0 = all OK
  - 1 = warnings present
  - 2 = critical failures
- `--quiet` mode that only shows violations

**Use cases**:
- Integration with monitoring scripts
- Automated health checks
- Quick visual identification of problems
- CI/CD health gates

**Implementation notes**:
- Store thresholds in configuration structure
- Apply color coding based on thresholds
- Count violations for exit code
- Consider warning vs critical levels

---

#### 4. Aggregate Statistics
**Description**: Show cluster-wide aggregate metrics.

**Proposed features**:
- Summary row showing: Avg CPU, Avg Mem, Max Load, Total nodes
- Per-group aggregations when filtering by group
- Cluster health score (0-100)
- Show outliers (nodes significantly above/below average)

**Use cases**:
- Quick cluster health overview
- Capacity planning
- Identifying anomalies
- Trend analysis

**Implementation notes**:
- Calculate stats after all fetches complete
- Show at table footer or dedicated section
- Consider percentile metrics (p50, p95, p99)
- Health score based on weighted factors

---

### Medium Priority - Infrastructure

#### 5. Configuration File
**Description**: Persistent configuration for user preferences.

**Proposed locations**:
- `~/.nodestatus/config` - User-level config
- `~/.config/nodestatus/config` - XDG compliant
- `/etc/nodestatus/config` - System-level defaults
- Per-inventory override: `config/inventory.yaml` can include config section

**Proposed settings**:
```yaml
defaults:
  sort: state
  colors: true
  timeout_ms: 500
  
thresholds:
  cpu_warn: 70
  cpu_crit: 90
  mem_warn: 80
  mem_crit: 95
  load_factor: 1.5  # multiply by CPU count
  
display:
  refresh_interval_ms: 1000
  decimal_places: 1
```

**Implementation notes**:
- Parse config on startup before CLI args
- CLI args override config values
- Use simple key=value or YAML format
- Validate config and show warnings for invalid entries

---

#### 6. IPv6 Support
**Description**: Handle IPv6 addresses in inventory and connections.

**Current limitation**: IPv6 addresses contain `:` which conflicts with port separator.

**Proposed format**:
- `[::1]:9002` - Bracket notation for IPv6 with port
- `::1` - IPv6 without port (use default)
- `[2001:db8::1]` - IPv6 without port, explicit brackets

**Implementation notes**:
- Update `parse_host_and_port()` to detect brackets
- Handle both IPv4 and IPv6 in `getaddrinfo()`
- Test with local IPv6 loopback and real addresses
- Update inventory examples in README

---

#### 7. Historical Tracking
**Description**: Track metrics over time for trend analysis.

**Proposed features**:
- Log mode: `nodectl log --interval 60` - Log metrics every 60 seconds
- Simple CSV or JSON lines format
- Show deltas: `CPU: 45% (+5%)` - change from N seconds ago
- Simple trend indicators: ↑ ↓ → (increasing, decreasing, stable)

**Storage format**:
```
timestamp,hostname,state,cpu,mem,load,disk
1710879401,localhost:9002,OK,45.2,67.8,1.23,12.45
```

**Use cases**:
- Detecting gradual resource drift
- Correlating issues with time
- Simple time-series data for graphing
- Capacity trend analysis

**Implementation notes**:
- Keep it simple - don't build a full TSDB
- Rotate logs by size or age
- Consider compression for old data
- Provide simple query tool: `nodectl history --last 1h --host server1`

---

### Advanced Features

#### 8. Custom Commands
**Description**: Allow users to define custom metrics via external scripts.

**Proposed config**:
```yaml
custom_metrics:
  gpu_temp:
    command: "/usr/local/bin/gpu-temp.sh"
    timeout: 2000
    type: gauge
  disk_smart:
    command: "/usr/sbin/smartctl -A /dev/sda | grep Temperature"
    parser: "regex"
    pattern: "Temperature.*?(\\d+)"
```

**Use cases**:
- Hardware-specific metrics (GPU, RAID, temperature sensors)
- Application-specific metrics
- Custom business logic
- Integration with existing monitoring scripts

**Implementation notes**:
- Execute commands with timeout
- Parse stdout for metric values
- Support JSON, plain text, and regex parsers
- Show in separate section or drill-down view

---

#### 9. Multi-endpoint Nodes
**Description**: Query multiple endpoints per node.

**Proposed features**:
- Primary endpoint: `/status` (current metrics)
- Secondary endpoints: `/app/metrics`, `/custom`
- Combine data from multiple sources
- Fallback to secondary if primary fails

**Use cases**:
- Nodes running multiple services
- Application-specific metrics alongside system metrics
- Redundant monitoring paths
- Migration scenarios (old + new agent)

**Implementation notes**:
- Extend Node struct with endpoint array
- Parallel fetch of multiple endpoints
- Merge strategy for conflicting data
- Timeout and error handling per endpoint

---

#### 10. Authentication
**Description**: Support for secured agent endpoints.

**Proposed methods**:
- Basic HTTP authentication
- Bearer token authentication
- Mutual TLS (client certificates)

**Configuration**:
```yaml
nodes:
  - hostname: secure-node
    port: 9002
    auth:
      type: bearer
      token: "${SECRET_TOKEN}"
      # or
      type: basic
      username: admin
      password: "${SECRET_PASS}"
```

**Implementation notes**:
- Support environment variable expansion for secrets
- Never log or display credentials
- Add TLS/SSL support alongside auth
- Verify certificates or allow `--insecure` flag

---

#### 11. Agent Health & Metadata
**Description**: Expose agent version, uptime, and self-health metrics.

**New agent endpoint**: `/health`
```json
{
  "version": "0.1.0",
  "uptime_seconds": 86400,
  "samples_collected": 86400,
  "last_sample_success": true,
  "platform": "Linux 5.15.0",
  "arch": "x86_64"
}
```

**Use cases**:
- Verify agent version across cluster
- Detect stale or crashed agents (check uptime)
- Agent deployment verification
- Debugging agent issues

---

#### 12. Parallel Inventory Loading
**Description**: Support multiple inventory sources simultaneously.

**Proposed syntax**:
```bash
nodectl status --inventory cluster1.yaml --inventory cluster2.yaml
# or
nodectl status --inventory-dir /etc/nodestatus/inventories/
```

**Use cases**:
- Large organizations with multiple inventory sources
- Combining different administrative domains
- Testing/prod inventory separation
- Dynamic inventory discovery

---

#### 13. Plugin System
**Description**: Allow third-party extensions via plugin architecture.

**Proposed features**:
- Plugins can add new views (like --cpu, --mem)
- Plugins can add new parsers
- Plugins can add new exporters
- Simple C-based plugin API or external process plugins

**Example plugin structure**:
```
~/.nodestatus/plugins/
  gpu-monitor.so
  kubernetes.so
```

**Implementation notes**:
- Start simple: external process plugins (stdin/stdout)
- Later: shared library plugins with defined API
- Plugin discovery and loading mechanism
- Sandboxing and safety considerations

---

## Non-goals

Features explicitly out of scope:

- **Full TSDB**: Use dedicated tools (Prometheus, InfluxDB) for long-term storage
- **Alerting system**: Integrate with existing tools (Alertmanager, PagerDuty)
- **Web UI**: CLI-focused tool; use Grafana for visualization
- **Agent auto-deployment**: Use existing config management (Ansible, Puppet)
- **Log aggregation**: Use dedicated tools (ELK, Loki)
- **Distributed consensus**: No cluster coordination or leader election

---

## Implementation Priority

Recommended order based on impact and complexity:

**Phase 1 - Core Enhancements**:
1. Filtering & Search
2. JSON/CSV Export
3. IPv6 Support

**Phase 2 - Usability**:
4. Configuration file
5. Thresholds & Alerts
6. Aggregate statistics

**Phase 3 - Advanced**:
7. Historical tracking (basic)
8. Agent health endpoint
9. Custom commands

**Phase 4 - Enterprise**:
10. Authentication
11. Multi-endpoint nodes
12. Plugin system

---

## Contributing

When implementing features from this roadmap:

1. Create feature branch: `feature/filter-nodes`
2. Update this document with implementation status
3. Add tests for new functionality
4. Update README.md with new flags/options
5. Keep changes focused and incremental
6. Maintain zero external dependencies policy where possible

---

## Feedback

Have suggestions for the roadmap? Open an issue or discussion on the repository.
