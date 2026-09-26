# Changelog

## 13.2.0

- Support detected field values in dashboard variables ([#177](https://github.com/grafana/grafana-loki-datasource/pull/177))
- Fix: drop query statistics from empty frames ([#192](https://github.com/grafana/grafana-loki-datasource/pull/192))
- Fix: do not keep any extra frame when the response is empty ([#193](https://github.com/grafana/grafana-loki-datasource/pull/193))
- Resolve high-severity CVEs in `golang.org/x/crypto`, `browserslist`, and `fast-uri` ([#191](https://github.com/grafana/grafana-loki-datasource/pull/191), [#129](https://github.com/grafana/grafana-loki-datasource/pull/129))

## 13.1.1

- Bump go v1.26.7 and grafana-plugin-sdk-go v0.296.4  ([#134](https://github.com/grafana/grafana-loki-datasource/pull/134))
- Chore: update lezer/logql ([#186](https://github.com/grafana/grafana-loki-datasource/pull/186))
- Removing abstraction related logic ([#182](https://github.com/grafana/grafana-loki-datasource/pull/182))
- chore: bump dependencies for vuln fixes ([#183](https://github.com/grafana/grafana-loki-datasource/pull/183))
- Tests: Update frontend datetime tests not to expect Moment internals ([#179](https://github.com/grafana/grafana-loki-datasource/pull/179))

## 13.1.0

- Persist disabled operations in Loki query builder ([#139](https://github.com/grafana/grafana-loki-datasource/pull/139))
- Bump @grafana/\* to 13.1.1 and update dependencies ([#137](https://github.com/grafana/grafana-loki-datasource/pull/137))

## 13.0.2

- Streaming key updates ([#135](https://github.com/grafana/grafana-loki-datasource/pull/135))
- Remove unused Rudderstack events ([#130](https://github.com/grafana/grafana-loki-datasource/pull/130))
- Update backend dependencies ([#120](https://github.com/grafana/grafana-loki-datasource/pull/120))
- Update frontend dependencies ([#122](https://github.com/grafana/grafana-loki-datasource/pull/122))
- Make grafanaDependency prerelease-inclusive ([#126](https://github.com/grafana/grafana-loki-datasource/pull/126))

## 13.0.1

- Bump grafana-plugin-sdk-go v0.292.2 ([#124](https://github.com/grafana/grafana-loki-datasource/pull/124))
- Pin frontend dependencies ([#121](https://github.com/grafana/grafana-loki-datasource/pull/121))
- Sync with grafana/grafana ([#115](https://github.com/grafana/grafana-loki-datasource/pull/115))

## 13.0.0

- Initial public release.
