#!/bin/bash
# Clone OSPREY repository to EC2 instance

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Configuration
INSTANCE_IP="${INSTANCE_IP:-}"
KEY_FILE="${KEY_FILE:-$HOME/.ssh/osprey-dev.pem}"
GITHUB_PAT="${GITHUB_PAT:-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}"
GITHUB_REPO="https://${GITHUB_PAT}@github.com/jasonlarkin/OSPREY3.git"

if [ -z "$INSTANCE_IP" ]; then
    # Try to read from saved file
    if [ -f "$REPO_ROOT/aws_instance_ip.txt" ]; then
        INSTANCE_IP=$(cat "$REPO_ROOT/aws_instance_ip.txt")
    else
        echo "Error: INSTANCE_IP not set and aws_instance_ip.txt not found"
        echo "Usage: INSTANCE_IP=<ip> $0"
        exit 1
    fi
fi

if [ ! -f "$KEY_FILE" ]; then
    echo "Error: Key file not found: $KEY_FILE"
    exit 1
fi

echo "=== Cloning OSPREY Repository to EC2 Instance ==="
echo "Instance IP: $INSTANCE_IP"
echo "Repository: jasonlarkin/OSPREY3"
echo ""

# Clone repository
echo "Cloning repository..."
ssh -i "$KEY_FILE" ec2-user@"$INSTANCE_IP" << EOF
cd /home/ec2-user/osprey-fork_modern
if [ -d .git ]; then
    echo "Repository already exists, pulling latest changes..."
    git pull
else
    echo "Cloning repository..."
    git clone $GITHUB_REPO .
fi
echo "Repository cloned successfully"
ls -la
EOF

echo ""
echo "=== Clone Complete ==="
echo "SSH into instance to build:"
echo "  ssh -i $KEY_FILE ec2-user@$INSTANCE_IP"
echo "  cd /home/ec2-user/osprey-fork_modern"
echo "  ./gradlew build"

