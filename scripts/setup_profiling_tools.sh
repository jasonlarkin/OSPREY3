#!/bin/bash
# Setup profiling tools for WSL environment
# Installs perf and async-profiler for CPU/memory profiling

set -euo pipefail

echo "=== Setting up Profiling Tools ==="

# Install perf for WSL
echo "Installing perf..."
if ! command -v perf &> /dev/null; then
    echo "Attempting to install perf for WSL kernel..."
    
    KERNEL_VERSION=$(uname -r)
    echo "Kernel version: $KERNEL_VERSION"
    
    # Try to install perf
    if sudo apt-get update && sudo apt-get install -y linux-tools-generic linux-tools-$(uname -r) 2>&1 | tee /tmp/perf_install.log; then
        echo "perf installed successfully"
    else
        echo "Standard perf installation failed. Trying alternative..."
        echo "You may need to manually install:"
        echo "  sudo apt-get install linux-tools-${KERNEL_VERSION}"
        echo "  or"
        echo "  sudo apt-get install linux-tools-standard-WSL2"
    fi
else
    echo "perf already installed"
fi

# Verify perf
if command -v perf &> /dev/null; then
    echo "perf version:"
    perf --version || true
else
    echo "WARNING: perf not available. CPU profiling will be limited."
fi

# Setup async-profiler (better for Java)
ASYNC_PROFILER_DIR="$HOME/async-profiler"
ASYNC_PROFILER_URL="https://github.com/async-profiler/async-profiler/releases/download/v2.9/async-profiler-2.9-linux-x64.tar.gz"

if [ ! -f "$ASYNC_PROFILER_DIR/async-profiler.jar" ]; then
    echo ""
    echo "Setting up async-profiler..."
    mkdir -p "$ASYNC_PROFILER_DIR"
    cd "$ASYNC_PROFILER_DIR" || exit 1
    
    if command -v wget &> /dev/null; then
        wget -q "$ASYNC_PROFILER_URL" -O async-profiler.tar.gz
    elif command -v curl &> /dev/null; then
        curl -L -o async-profiler.tar.gz "$ASYNC_PROFILER_URL"
    else
        echo "ERROR: Need wget or curl to download async-profiler"
        exit 1
    fi
    
    tar -xzf async-profiler.tar.gz --strip-components=1
    rm async-profiler.tar.gz
    
    echo "async-profiler installed to: $ASYNC_PROFILER_DIR"
    echo "Usage: java -jar $ASYNC_PROFILER_DIR/async-profiler.jar -e cpu -d 60 -f cpu.html <pid>"
else
    echo "async-profiler already installed at: $ASYNC_PROFILER_DIR"
fi

echo ""
echo "=== Profiling Tools Setup Complete ==="
echo ""
echo "Available tools:"
if command -v perf &> /dev/null; then
    echo "  - perf: $(which perf)"
fi
if [ -f "$ASYNC_PROFILER_DIR/async-profiler.jar" ]; then
    echo "  - async-profiler: $ASYNC_PROFILER_DIR/async-profiler.jar"
fi

