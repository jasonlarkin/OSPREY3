#!/bin/bash
# Start SLURM services in background (for WSL)

set -euo pipefail

echo "=== Starting SLURM services in background ==="

# Stop any existing services
sudo pkill -9 slurmctld 2>/dev/null || true
sudo pkill -9 slurmd 2>/dev/null || true
sleep 1

# Ensure state directories exist
sudo mkdir -p /tmp/slurmctld /tmp/slurmd
sudo chmod 777 /tmp/slurmctld /tmp/slurmd

# Start slurmctld in background
echo "Starting slurmctld..."
sudo slurmctld -D > /tmp/slurmctld.out 2>&1 &
SLURMCTLD_PID=$!
sleep 2

# Check if slurmctld started successfully
if ! ps -p $SLURMCTLD_PID > /dev/null; then
    echo "ERROR: slurmctld failed to start. Check /tmp/slurmctld.out"
    cat /tmp/slurmctld.out
    exit 1
fi

# Start slurmd in background
echo "Starting slurmd..."
sudo slurmd -D > /tmp/slurmd.out 2>&1 &
SLURMD_PID=$!
sleep 2

# Check if slurmd started successfully
if ! ps -p $SLURMD_PID > /dev/null; then
    echo "ERROR: slurmd failed to start. Check /tmp/slurmd.out"
    cat /tmp/slurmd.out
    sudo pkill -9 slurmctld
    exit 1
fi

echo ""
echo "SLURM services started:"
echo "  slurmctld PID: $SLURMCTLD_PID"
echo "  slurmd PID: $SLURMD_PID"
echo ""
echo "Logs:"
echo "  slurmctld: /tmp/slurmctld.out"
echo "  slurmd: /tmp/slurmd.out"
echo ""
echo "Verify with:"
echo "  sinfo"
echo "  srun hostname"
echo ""
echo "Stop with:"
echo "  ./scripts/stop_slurm_local.sh"
echo ""

