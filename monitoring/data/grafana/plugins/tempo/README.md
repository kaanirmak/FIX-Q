# Tempo data source for Grafana

> **Note**: This core plugin was extracted from the [grafana/grafana](https://github.com/grafana/grafana) repository
> and is now bundled with Grafana.

## Overview

Grafana datasource for the Tempo distributed tracing backend — Grafana Labs' high-volume,
minimal-dependency open-source tracing system. The plugin lets you query and visualize traces
stored in Tempo, including TraceQL, Search, Service Graph, and TraceID-based lookups.

## Requirements

- Grafana 12.3.0 or later

## Getting started

This plugin is bundled with Grafana — no installation is required for standard Grafana deployments.

1. Navigate to **Connections > Data sources** in Grafana.
2. Click **Add data source** and search for "Tempo".
3. Configure the connection settings and click **Save & test**.

For detailed setup instructions, see the
[Tempo data source documentation](https://grafana.com/docs/grafana/latest/datasources/tempo/).

### Custom Grafana distributions

If you are building a custom Grafana binary or distribution that excludes bundled plugins,
you can install this plugin from the [Grafana plugin catalog](https://grafana.com/grafana/plugins/).

## Documentation

Full documentation is available at:

https://grafana.com/docs/grafana/latest/datasources/tempo/

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md).

## License

This plugin is licensed under the [AGPL-3.0](LICENSE).
