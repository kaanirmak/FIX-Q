# Loki data source for Grafana

> **Note**: This core plugin was extracted from the [grafana/grafana](https://github.com/grafana/grafana) repository
> and is now bundled with Grafana.

## Overview

The Loki data source plugin allows you to query and visualize log data from [Grafana Loki](https://grafana.com/oss/loki/), a horizontally scalable, highly available log aggregation system.

## Requirements

- Grafana 12.3 or later

## Getting started

This plugin is bundled with Grafana — no installation is required for standard Grafana deployments.

1. Navigate to **Connections > Data sources** in Grafana.
2. Click **Add data source** and search for "Loki".
3. Configure the connection settings and click **Save & test**.

For detailed setup instructions, see the
[Loki data source documentation](https://grafana.com/docs/grafana/latest/datasources/loki/).

### Custom Grafana distributions

If you are building a custom Grafana binary or distribution that excludes bundled plugins,
you can install this plugin from the [Grafana plugin catalog](https://grafana.com/grafana/plugins/).

## Documentation

Full documentation is available at:

https://grafana.com/docs/grafana/latest/datasources/loki/

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md).

## License

This plugin is licensed under the [AGPL-3.0](LICENSE).
