#!/bin/bash
# Start SLURM services locally (WSL-friendly, no systemd)
# NOTE: If you get password prompt issues, run commands manually:
#   Terminal 1: sudo slurmctld -D
#   Terminal 2: sudo slurmd -D

set -euo pipefail

SLURM_CONF="${SLURM_CONF:-/etc/slurm/slurm.conf}"

if [ ! -f "$SLURM_CONF" ]; then
    echo "Error: SLURM configuration not found at $SLURM_CONF"
    echo "Run: ./scripts/setup_slurm_local_wsl.sh"
    exit 1
fi

echo "=== Starting SLURM services ==="
echo ""
echo "If sudo password prompt doesn't work, run commands manually:"
echo "  Terminal 1: sudo slurmctld -D"
echo "  Terminal 2: sudo slurmd -D"
echo ""

# Check if already running
if pgrep -x slurmctld > /dev/null; then
    echo "slurmctld is already running (PID: $(pgrep -x slurmctld))"
else
    echo "Starting slurmctld (will prompt for sudo password)..."
    sudo -v  # Verify sudo access first
    sudo nohup slurmctld -D > /tmp/slurmctld_startup.log 2>&1 &
    sleep 3
    if pgrep -x slurmctld > /dev/null; then
        echo "slurmctld started (PID: $(pgrep -x slurmctld))"
    else
        echo "Error: Failed to start slurmctld"
        echo "Check: cat /tmp/slurmctld_startup.log"
        echo "Or run manually: sudo slurmctld -D"
        exit 1
    fi
fi

if pgrep -x slurmd > /dev/null; then
    echo "slurmd is already running (PID: $(pgrep -x slurmd))"
else
    echo "Starting slurmd (will prompt for sudo password)..."
    sudo -v  # Verify sudo access first
    sudo nohup slurmd -D > /tmp/slurmd_startup.log 2>&1 &
    sleep 3
    if pgrep -x slurmd > /dev/null; then
        echo "slurmd started (PID: $(pgrep -x slurmd))"
    else
        echo "Error: Failed to start slurmd"
        echo "Check: cat /tmp/slurmd_startup.log"
        echo "Or run manually: sudo slurmd -D"
        exit 1
    fi
fi

# Wait for services to be ready
sleep 2

# Check status
echo ""
echo "SLURM status:"
sinfo 2>/dev/null || echo "Warning: sinfo failed - services may still be starting"

echo ""
echo "To stop SLURM services:"
echo "  sudo pkill slurmctld"
echo "  sudo pkill slurmd"
echo ""

