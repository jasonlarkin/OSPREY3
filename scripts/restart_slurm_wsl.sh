#!/bin/bash
# Restart SLURM services with fixed WSL configuration

set -euo pipefail

echo "=== Restarting SLURM with WSL configuration ==="

# Stop any running services
echo "Stopping existing SLURM services..."
sudo pkill -9 slurmctld 2>/dev/null || true
sudo pkill -9 slurmd 2>/dev/null || true
sleep 2

# Create state directories
echo "Creating state directories..."
sudo mkdir -p /tmp/slurmctld /tmp/slurmd
sudo chmod 777 /tmp/slurmctld /tmp/slurmd

# Verify configuration
echo ""
echo "Verifying node configuration..."
sudo slurmd -C

echo ""
echo "=== Configuration looks good ==="
echo ""
echo "Now start SLURM in separate terminals:"
echo ""
echo "Terminal 1 (slurmctld):"
echo "  sudo slurmctld -D"
echo ""
echo "Terminal 2 (slurmd):"
echo "  sudo slurmd -D"
echo ""
echo "Terminal 3 (verify):"
echo "  sinfo"
echo "  srun hostname"
echo ""

