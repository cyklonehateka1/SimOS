FROM ubuntu:22.04

# Install system dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    libyaml-dev \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /app

# Copy source code into container
COPY . .

# Build SimOS
RUN make

# Expose any port if needed (optional)
# EXPOSE 8080

# Run SimOS when container starts
CMD ["./simos"]
