# Changelog

## 13.2.2

- Fix security vulnerabilities (CVE-2026-73088, CVE-2026-73089, CVE-2026-75899, CVE-2026-75931, CVE-2026-75975, CVE-2026-76172, CVE-2026-84375)

## 13.2.1

- Dependency updates ([#226](https://github.com/grafana/grafana-tempo-datasource/pull/226))
- Fix TraceQL instant metrics queries failing when Tempo returns new fields ([#247](https://github.com/grafana/grafana-tempo-datasource/pull/227))

## 13.2.0

- Bump go v1.26.7 and grafana-plugin-sdk-go v0.296.4 ([#229](https://github.com/grafana/grafana-tempo-datasource/pull/220))
- Fix search query error details not propagated to user ([#203](https://github.com/grafana/grafana-tempo-datasource/pull/203))
- Search: Allow a custom label for static search fields ([#205](https://github.com/grafana/grafana-tempo-datasource/pull/205))
- Return a friendly error instead of raw HTML on non-2xx Tempo responses ([#214](https://github.com/grafana/grafana-tempo-datasource/pull/214))
- Do not escape a single value regex in TraceQL search filters ([#211](https://github.com/grafana/grafana-tempo-datasource/pull/211))
- Fix: Show query error details for unsupported query types instead of a generic plugin error ([#222](https://github.com/grafana/grafana-tempo-datasource/pull/222))
- Update dependencies ([#232](https://github.com/grafana/grafana-tempo-datasource/pull/232))
- Tempo: Normalize provisioned timeRangeForTags duration strings ([#234](https://github.com/grafana/grafana-tempo-datasource/pull/234))
- Return a clearer error when a trace is not found in the time range ([#213](https://github.com/grafana/grafana-tempo-datasource/pull/213))
- Fix: Avoid double slash in trace and metrics URLs ([#212](https://github.com/grafana/grafana-tempo-datasource/pull/212))
- Tempo: Quote custom search values when tag value type is unknown ([#224](https://github.com/grafana/grafana-tempo-datasource/pull/224))
- Tempo: Fix options row padding ([#238](https://github.com/grafana/grafana-tempo-datasource/pull/238))
- Add valueType to getTagValues results ([#239](https://github.com/grafana/grafana-tempo-datasource/pull/239))
- Fix: Keep last metrics payload on streaming Done ([#237](https://github.com/grafana/grafana-tempo-datasource/pull/237))


## 13.1.5

- Fix: Avoid duplicate X-Scope-OrgID header on streaming search ([#207](https://github.com/grafana/grafana-tempo-datasource/pull/207))

## 13.1.4

- Fix: Case-insensitive header collision when forwarding team headers ([#199](https://github.com/grafana/grafana-tempo-datasource/pull/199))

## 13.1.3

- Fix path traversal GL-Vuln: VUL-2026-0062 ([#197](https://github.com/grafana/grafana-tempo-datasource/pull/197))

## 13.1.2

- Bump go v1.26.4
- Search: Unify nested span subframe schema across span sets
- Update various backend and frontend dependencies

## 13.1.1

- Minor improvements and bug fixes
