#!/bin/bash
# Setup SLURM locally for WSL/testing OSPREY pipeline
# Uses SLURM without systemd (WSL-friendly)

set -euo pipefail

echo "=== Setting up SLURM locally (WSL-friendly) ==="

# Check if SLURM is already installed
if command -v sbatch &> /dev/null; then
    echo "SLURM appears to be already installed"
    sbatch --version
    exit 0
fi

# Check if running on WSL
IS_WSL=false
if [ -f /proc/version ] && (grep -qiE "(microsoft|wsl)" /proc/version); then
    IS_WSL=true
    echo "Detected WSL environment"
fi

# Detect distribution
if [ -f /etc/os-release ]; then
    . /etc/os-release
    DISTRO=$ID
else
    DISTRO=$(lsb_release -si 2>/dev/null || echo "ubuntu")
fi

echo "Detected distribution: $DISTRO"

# Install SLURM
if [[ "$DISTRO" == "ubuntu" ]] || [[ "$DISTRO" == "debian" ]]; then
    echo "Installing SLURM..."
    sudo apt-get update
    sudo apt-get install -y slurm-wlm slurm-wlm-doc
    
    # Create minimal SLURM configuration
    SLURM_CONF="/etc/slurm/slurm.conf"
    if [ ! -f "$SLURM_CONF" ]; then
        echo "Creating SLURM configuration..."
        sudo mkdir -p /etc/slurm
        
        # Get CPU count and memory
        CPU_COUNT=$(nproc)
        MEM_GB=$(free -g | awk '/^Mem:/{print $2}')
        MEM_MB=$((MEM_GB * 1024))
        
        sudo tee "$SLURM_CONF" > /dev/null <<EOF
# Minimal SLURM configuration for local development
ClusterName=local
ControlMachine=$(hostname)
ControlAddr=127.0.0.1
SlurmUser=$USER
SlurmdUser=root
SlurmctldPort=6817
SlurmdPort=6818
AuthType=auth/none
StateSaveLocation=/tmp/slurmctld
SlurmdSpoolDir=/tmp/slurmd
SwitchType=switch/none
MpiDefault=none
SlurmctldPidFile=/tmp/slurmctld.pid
SlurmdPidFile=/tmp/slurmd.pid
SlurmctldLogFile=/tmp/slurmctld.log
SlurmdLogFile=/tmp/slurmd.log
NodeName=localhost CPUs=${CPU_COUNT} RealMemory=${MEM_MB} State=UNKNOWN
PartitionName=debug Nodes=localhost Default=YES MaxTime=INFINITE MaxNodes=1
EOF
        
        echo "SLURM configuration created at $SLURM_CONF"
    fi
    
    # Create spool directories (use /tmp instead of /var/spool for WSL)
    sudo mkdir -p /tmp/slurmctld
    sudo mkdir -p /tmp/slurmd
    sudo chown $USER:$USER /tmp/slurmctld 2>/dev/null || sudo chmod 777 /tmp/slurmctld
    sudo chown $USER:$USER /tmp/slurmd 2>/dev/null || sudo chmod 777 /tmp/slurmd
    
    echo ""
    echo "=== SLURM Installation Complete ==="
    echo ""
    echo "To start SLURM (without systemd):"
    echo "  sudo slurmd -C  # Check configuration"
    echo "  sudo slurmctld  # Start controller (in background or new terminal)"
    echo "  sudo slurmd     # Start daemon (in background or new terminal)"
    echo ""
    echo "Or use the helper script:"
    echo "  ./scripts/start_slurm_local.sh"
    echo ""
    
elif [[ "$DISTRO" == "rhel" ]] || [[ "$DISTRO" == "centos" ]] || [[ "$DISTRO" == "rocky" ]] || [[ "$DISTRO" == "fedora" ]]; then
    echo "Installing SLURM on RHEL-based system..."
    sudo yum install -y slurm slurm-devel slurm-perlapi
    
    echo "Please configure SLURM manually for RHEL-based systems"
    echo "Configuration file: /etc/slurm/slurm.conf"
else
    echo "Unsupported distribution: $DISTRO"
    echo "Please install SLURM manually"
    exit 1
fi

echo ""
echo "Test SLURM with:"
echo "  sinfo                    # Show cluster info"
echo "  srun hostname            # Run a simple job"
echo "  sbatch --wrap='sleep 10' # Submit a batch job"
echo ""

