#!/bin/bash
# Fix AmberTools GLIBC compatibility issue on EC2 instance

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Configuration
INSTANCE_IP="${INSTANCE_IP:-}"
KEY_FILE="${KEY_FILE:-$HOME/.ssh/osprey-dev.pem}"

if [ -z "$INSTANCE_IP" ]; then
    if [ -f "$REPO_ROOT/aws_instance_ip.txt" ]; then
        INSTANCE_IP=$(cat "$REPO_ROOT/aws_instance_ip.txt")
    else
        echo "Error: INSTANCE_IP not set and aws_instance_ip.txt not found"
        exit 1
    fi
fi

if [ ! -f "$KEY_FILE" ]; then
    echo "Error: Key file not found: $KEY_FILE"
    exit 1
fi

echo "=== Checking GLIBC Version on EC2 Instance ==="
echo "Instance IP: $INSTANCE_IP"
echo ""

# Check GLIBC version
GLIBC_VERSION=$(ssh -i "$KEY_FILE" ec2-user@"$INSTANCE_IP" "ldd --version | head -1 | grep -oE '[0-9]+\.[0-9]+'")
echo "Current GLIBC version: $GLIBC_VERSION"

if [ "$(echo "$GLIBC_VERSION < 2.27" | bc -l)" -eq 1 ]; then
    echo ""
    echo "GLIBC version is too old for pre-compiled AmberTools binaries."
    echo ""
    echo "Options:"
    echo "1. Skip the failing test (quickest):"
    echo "   ./gradlew build -x test"
    echo ""
    echo "2. Install AmberTools from source (takes longer):"
    echo "   This requires downloading and compiling AmberTools"
    echo ""
    echo "3. Use Amazon Linux 2023 (recommended for new instances):"
    echo "   Has GLIBC 2.34+ and better compatibility"
    echo ""
    echo "For now, you can continue with the build by skipping tests:"
    echo "  ssh -i $KEY_FILE ec2-user@$INSTANCE_IP"
    echo "  cd /home/ec2-user/osprey-fork_modern"
    echo "  ./gradlew build -x test"
else
    echo "GLIBC version should be compatible."
fi

