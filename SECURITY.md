# Security policy

Window Tidy is a local-only Win32 utility. It does not install drivers, services, hooks or agents, collect telemetry, or contact remote servers.

## Reporting

Please report suspected security vulnerabilities privately through GitHub's repository security advisory channel when available. Otherwise contact the repository owner privately. Do not disclose working exploits in a public issue before remediation.

## Trust boundaries

- Window titles, class names, HWNDs and registry data may be controlled by other local processes.
- No administrator privilege is requested. Cross-integrity operations may fail and must not be forcibly bypassed.
- The app never reads clipboard contents or other processes' memory.
- Registry writes are limited to HKCU WindowTidy configuration and the explicit HKCU Run entry.
- The hotkey implementation uses RegisterHotKey. It does not install keyboard hooks or perform keylogging.
- GitHub Actions artifacts are not code-signed releases. Verify provenance and hashes independently.
