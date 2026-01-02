#!/bin/bash
# Setup OSPREY on a provisioned EC2 instance

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Configuration
INSTANCE_IP="${INSTANCE_IP:-}"
KEY_FILE="${KEY_FILE:-$HOME/.ssh/osprey-dev.pem}"

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

echo "=== Setting up OSPREY on EC2 Instance ==="
echo "Instance IP: $INSTANCE_IP"
echo "Key file: $KEY_FILE"
echo ""

# Wait for instance to be ready (SSH accessible)
echo "Waiting for SSH to be available..."
for i in {1..30}; do
    if ssh -i "$KEY_FILE" -o StrictHostKeyChecking=no -o ConnectTimeout=5 ec2-user@"$INSTANCE_IP" "echo 'SSH ready'" &> /dev/null; then
        echo "SSH is ready"
        break
    fi
    if [ $i -eq 30 ]; then
        echo "Error: SSH not available after 5 minutes"
        exit 1
    fi
    echo "Waiting... ($i/30)"
    sleep 10
done

# Wait for setup to complete
echo "Waiting for instance setup to complete..."
ssh -i "$KEY_FILE" ec2-user@"$INSTANCE_IP" "while [ ! -f /var/log/osprey-setup.log ] || ! grep -q 'OSPREY development instance setup complete' /var/log/osprey-setup.log; do sleep 5; done; echo 'Setup complete'; tail -20 /var/log/osprey-setup.log" || true

# Create setup script on remote instance
cat > /tmp/setup_osprey_remote.sh << 'REMOTE_EOF'
#!/bin/bash
set -e

echo "=== OSPREY Setup on EC2 ==="

# Create osprey directory
OSPREY_DIR="/home/ec2-user/osprey-fork_modern"
mkdir -p "$OSPREY_DIR"
cd "$OSPREY_DIR"

# Clone repository (or user will upload)
echo "Repository directory: $OSPREY_DIR"
echo "You can:"
echo "  1. Clone from git: git clone <repo-url> ."
echo "  2. Or upload files via scp/rsync"

# Setup Python virtual environment (portable, non-proprietary)
python3 -m venv /home/ec2-user/osprey_tools_venv
source /home/ec2-user/osprey_tools_venv/bin/activate

# Install Python dependencies
pip install --upgrade pip

# Install uv via official installer (works with any Python version)
curl -LsSf https://astral.sh/uv/install.sh | sh
export PATH="$HOME/.local/bin:$PATH"
source "$HOME/.local/bin/env" 2>/dev/null || true

# Install OSPREY Python dependencies (if requirements.txt exists)
if [ -f requirements.txt ]; then
    pip install -r requirements.txt
fi

# Setup Java
export JAVA_HOME=/usr/lib/jvm/java-17-amazon-corretto
export PATH=$JAVA_HOME/bin:$PATH

# Verify Java
java -version

# Setup environment variables
cat >> /home/ec2-user/.bashrc << 'ENV_EOF'
export JAVA_HOME=/usr/lib/jvm/java-17-amazon-corretto
export PATH=$JAVA_HOME/bin:$HOME/.local/bin:$PATH
export OSPREY_HOME=/home/ec2-user/osprey-fork_modern
export OSPREY_TOOLS_VENV=/home/ec2-user/osprey_tools_venv
source /home/ec2-user/osprey_tools_venv/bin/activate
source $HOME/.local/bin/env 2>/dev/null || true
ENV_EOF

echo ""
echo "=== OSPREY Setup Complete ==="
echo "Next steps:"
echo "  1. Clone/upload OSPREY repository to $OSPREY_DIR"
echo "  2. Build OSPREY: cd $OSPREY_DIR && ./gradlew build"
echo "  3. Test installation: python3 -c 'import osprey; osprey.start()'"
REMOTE_EOF

# Copy setup script to instance
scp -i "$KEY_FILE" /tmp/setup_osprey_remote.sh ec2-user@"$INSTANCE_IP":/tmp/

# Run setup script
echo "Running OSPREY setup on instance..."
ssh -i "$KEY_FILE" ec2-user@"$INSTANCE_IP" "bash /tmp/setup_osprey_remote.sh"

echo ""
echo "=== Setup Complete ==="
echo "SSH into instance:"
echo "  ssh -i $KEY_FILE ec2-user@$INSTANCE_IP"
echo ""
echo "Next steps:"
echo "  1. Clone/upload OSPREY repository"
echo "  2. Build and test OSPREY"

