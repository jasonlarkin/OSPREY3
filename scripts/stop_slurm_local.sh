#!/bin/bash
# Stop SLURM services locally

set -euo pipefail

echo "=== Stopping SLURM services ==="

if pgrep -x slurmctld > /dev/null; then
    echo "Stopping slurmctld (PID: $(pgrep -x slurmctld))..."
    sudo pkill slurmctld
    sleep 1
else
    echo "slurmctld is not running"
fi

if pgrep -x slurmd > /dev/null; then
    echo "Stopping slurmd (PID: $(pgrep -x slurmd))..."
    sudo pkill slurmd
    sleep 1
else
    echo "slurmd is not running"
fi

echo "SLURM services stopped"

