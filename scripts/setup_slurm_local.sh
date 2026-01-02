#!/bin/bash
# Setup SLURM locally for testing OSPREY pipeline
# Mimics cluster environment for development

set -euo pipefail

echo "=== Setting up SLURM locally ==="

# Check if running on WSL
if [ -f /proc/version ] && (grep -q Microsoft /proc/version || grep -q WSL /proc/version || grep -q microsoft /proc/version); then
    echo "Detected WSL environment"
    DISTRO=$(grep -oP '(?<=Distributor ID:)\s*\K\w+' /etc/lsb-release 2>/dev/null || echo "Ubuntu")
else
    DISTRO=$(lsb_release -si 2>/dev/null || echo "Ubuntu")
fi

echo "Detected distribution: $DISTRO"

# Install SLURM
if [ "$DISTRO" = "Ubuntu" ] || [ "$DISTRO" = "Debian" ]; then
    echo "Installing SLURM..."
    sudo apt-get update
    sudo apt-get install -y slurm-wlm slurm-wlm-doc
    
    # Create minimal SLURM configuration
    SLURM_CONF="/etc/slurm/slurm.conf"
    if [ ! -f "$SLURM_CONF" ]; then
        echo "Creating SLURM configuration..."
        sudo mkdir -p /etc/slurm
        
        sudo tee "$SLURM_CONF" > /dev/null <<EOF
# Minimal SLURM configuration for local development
ClusterName=local
ControlMachine=localhost
ControlAddr=127.0.0.1
SlurmUser=slurm
SlurmdUser=root
SlurmctldPort=6817
SlurmdPort=6818
AuthType=auth/none
StateSaveLocation=/var/spool/slurmctld
SlurmdSpoolDir=/var/spool/slurmd
SwitchType=switch/none
MpiDefault=none
SlurmctldPidFile=/var/run/slurmctld.pid
SlurmdPidFile=/var/run/slurmd.pid
SlurmctldLogFile=/var/log/slurmctld.log
SlurmdLogFile=/var/log/slurmd.log
NodeName=localhost CPUs=4 RealMemory=8000 State=UNKNOWN
PartitionName=debug Nodes=localhost Default=YES MaxTime=INFINITE MaxNodes=1
EOF
        
        echo "SLURM configuration created at $SLURM_CONF"
    fi
    
    # Create spool directories
    sudo mkdir -p /var/spool/slurmctld
    sudo mkdir -p /var/spool/slurmd
    sudo chown slurm:slurm /var/spool/slurmctld
    sudo chown slurm:slurm /var/spool/slurmd
    
    # Start SLURM services
    echo "Starting SLURM services..."
    sudo systemctl enable slurmd
    sudo systemctl enable slurmctld
    sudo systemctl start slurmd
    sudo systemctl start slurmctld
    
    # Wait for services to start
    sleep 2
    
    # Check status
    if systemctl is-active --quiet slurmd && systemctl is-active --quiet slurmctld; then
        echo "SLURM services started successfully"
        sinfo
    else
        echo "Warning: SLURM services may not have started correctly"
        echo "Check status with: sudo systemctl status slurmd slurmctld"
    fi
    
elif [ "$DISTRO" = "CentOS" ] || [ "$DISTRO" = "Rocky" ] || [ "$DISTRO" = "RHEL" ]; then
    echo "Installing SLURM on RHEL-based system..."
    sudo yum install -y slurm slurm-devel slurm-perlapi
    
    # Similar configuration steps as Ubuntu
    echo "Please configure SLURM manually for RHEL-based systems"
else
    echo "Unsupported distribution: $DISTRO"
    echo "Please install SLURM manually"
    exit 1
fi

echo ""
echo "=== SLURM Setup Complete ==="
echo ""
echo "Test SLURM with:"
echo "  sinfo                    # Show cluster info"
echo "  srun hostname            # Run a simple job"
echo "  sbatch --wrap='sleep 10' # Submit a batch job"
echo ""
echo "For OSPREY pipeline testing:"
echo "  sbatch scripts/run_osprey_kstar.sh"

