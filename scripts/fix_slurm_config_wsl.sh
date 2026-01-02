#!/bin/bash
# Fix SLURM configuration for WSL (disable cgroups, fix node name)

set -euo pipefail

SLURM_CONF="/etc/slurm/slurm.conf"
HOSTNAME=$(hostname)
CPU_COUNT=$(nproc)
MEM_GB=$(free -g | awk '/^Mem:/{print $2}')
MEM_MB=$((MEM_GB * 1024))

echo "=== Fixing SLURM configuration for WSL ==="
echo "Hostname: $HOSTNAME"
echo "CPUs: $CPU_COUNT"
echo "Memory: ${MEM_MB}MB"
echo ""

# Backup existing config
if [ -f "$SLURM_CONF" ]; then
    sudo cp "$SLURM_CONF" "$SLURM_CONF.backup"
    echo "Backed up existing config to $SLURM_CONF.backup"
fi

# Create fixed configuration
sudo tee "$SLURM_CONF" > /dev/null <<EOF
# SLURM configuration for WSL (cgroups disabled)
ClusterName=local
ControlMachine=${HOSTNAME}
ControlAddr=127.0.0.1
SlurmUser=${USER}
SlurmdUser=root
SlurmctldPort=6817
SlurmdPort=6818
AuthType=auth/none
StateSaveLocation=/tmp/slurmctld
SlurmdSpoolDir=/tmp/slurmd
SwitchType=switch/none
MpiDefault=none
MailProg=/bin/true
SlurmctldPidFile=/tmp/slurmctld.pid
SlurmdPidFile=/tmp/slurmd.pid
SlurmctldLogFile=/tmp/slurmctld.log
SlurmdLogFile=/tmp/slurmd.log

# Disable cgroups (WSL doesn't support them)
ProctrackType=proctrack/pgid
TaskPlugin=task/none

# Node configuration (must match hostname from slurmd -C)
NodeName=${HOSTNAME} CPUs=${CPU_COUNT} RealMemory=${MEM_MB} State=UNKNOWN
PartitionName=debug Nodes=${HOSTNAME} Default=YES MaxTime=INFINITE MaxNodes=1
EOF

echo "Configuration updated at $SLURM_CONF"
echo ""
echo "Next steps:"
echo "1. Stop any running SLURM services:"
echo "   sudo pkill slurmctld; sudo pkill slurmd"
echo ""
echo "2. Create state directories:"
echo "   sudo mkdir -p /tmp/slurmctld /tmp/slurmd"
echo "   sudo chmod 777 /tmp/slurmctld /tmp/slurmd"
echo ""
echo "3. Verify node configuration matches:"
echo "   sudo slurmd -C"
echo ""
echo "4. Start SLURM:"
echo "   sudo slurmctld -D  # Terminal 1"
echo "   sudo slurmd -D     # Terminal 2"
echo ""

