# ==========================================
# Build Stage
# ==========================================
FROM ubuntu:22.04 AS builder

# Prevent interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Install build dependencies
RUN apt-get update && apt-get install -y \
    cmake \
    ninja-build \
    g++ \
    libboost-all-dev \
    libsqlite3-dev \
    libssl-dev \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /app

# Copy the entire workspace
COPY . .

# Configure and compile in Release mode
RUN cmake -S . -B build -GNinja -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --target mailforge

# ==========================================
# Run Stage
# ==========================================
FROM ubuntu:22.04

# Prevent interactive prompts
ENV DEBIAN_FRONTEND=noninteractive

# Install runtime dependencies (OpenSSL, SQLite3, Boost)
RUN apt-get update && apt-get install -y \
    libsqlite3-0 \
    libssl3 \
    libboost-system-dev \
    libboost-thread-dev \
    dnsutils \
    iproute2 \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /app

# Copy the compiled binary from the builder stage
COPY --from=builder /app/build/mailforge /app/mailforge

# Copy default config files
COPY --from=builder /app/config /app/config

# Create default directories for mail and database storage
RUN mkdir -p /app/mail /app/data

# Expose SMTP (2525) and POP3 (1100) ports
EXPOSE 2525 1100

# Set entry point
ENTRYPOINT ["/app/mailforge", "/app/config/mailforge.json"]
