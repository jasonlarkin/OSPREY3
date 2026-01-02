#!/bin/bash
# Manual SLURM start - run commands one at a time
# Copy and paste each command separately into terminal

echo "=== Manual SLURM Start Instructions ==="
echo ""
echo "Run these commands ONE AT A TIME in separate terminal sessions:"
echo ""
echo "1. Start slurmctld (controller) in Terminal 1:"
echo "   sudo slurmctld -D"
echo ""
echo "2. Start slurmd (daemon) in Terminal 2:"
echo "   sudo slurmd -D"
echo ""
echo "3. In Terminal 3, verify SLURM is running:"
echo "   sinfo"
echo "   srun hostname"
echo ""
echo "To stop: sudo pkill slurmctld && sudo pkill slurmd"

