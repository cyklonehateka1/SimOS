# Use Ubuntu base
FROM ubuntu:24.04

# Install build tools
RUN apt update && apt install -y build-essential cmake git

# Set work directory
WORKDIR /app

# Copy SimOS source code
COPY . .

# Build SimOS
RUN make   # or your build command

# Set default command to run SimOS
CMD ["./simos"]
