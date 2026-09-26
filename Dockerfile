# ─────────────────────────────────────────────────────────────
# FINORA-Q: Post-Quantum Cryptography & Multi-Domain Test Stack
# ─────────────────────────────────────────────────────────────
FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    g++ \
    make \
    libssl-dev \
    curl \
    ca-certificates \
    python3 \
    python3-pip \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY include/ ./include/
COPY src/ ./src/
COPY tools/ ./tools/
COPY examples/ ./examples/
COPY tests/ ./tests/
COPY config/ ./config/
COPY Makefile CMakeLists.txt ./

# Compile FINORA-Q Framework binaries (-O3 optimization)
RUN make prod full

# ─────────────────────────────────────────────────────────────
# Runtime Stage (Includes Prometheus + Grafana for Railway / Single-Container)
# ─────────────────────────────────────────────────────────────
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
ENV PYTHONUNBUFFERED=1

RUN apt-get update && apt-get install -y --no-install-recommends \
    libssl3 \
    ca-certificates \
    python3 \
    python3-pip \
    curl \
    gnupg \
    procps \
    lsof \
    prometheus \
    && mkdir -p /etc/apt/keyrings/ \
    && curl -fsSL https://apt.grafana.com/gpg.key | gpg --dearmor -o /etc/apt/keyrings/grafana.gpg \
    && echo "deb [signed-by=/etc/apt/keyrings/grafana.gpg] https://apt.grafana.com stable main" > /etc/apt/sources.list.d/grafana.list \
    && apt-get update \
    && apt-get install -y --no-install-recommends grafana \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy compiled binaries and libraries from builder
COPY --from=builder /app/bin/ /app/bin/
COPY --from=builder /app/lib/ /app/lib/
COPY --from=builder /app/include/ /app/include/

# Copy scripts, config, tools, and monitoring templates
COPY config/ /app/config/
COPY tools/ /app/tools/
COPY monitoring/ /app/monitoring/
COPY run_test.sh logs.sh status_all.sh stop_all.sh /app/

# Set up runtime directories
RUN mkdir -p /app/logs /app/monitoring/data/prometheus /app/monitoring/data/grafana && \
    chmod +x /app/bin/* /app/run_test.sh /app/logs.sh /app/status_all.sh /app/stop_all.sh

EXPOSE 3000 8080 9090 9100

COPY docker-entrypoint.sh /app/docker-entrypoint.sh
RUN chmod +x /app/docker-entrypoint.sh

ENTRYPOINT ["/app/docker-entrypoint.sh"]
CMD ["start"]
