#!/bin/bash
# Setup SLURM on AWS EC2 instance (Amazon Linux 2)

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Configuration
INSTANCE_IP="${INSTANCE_IP:-}"
KEY_FILE="${KEY_FILE:-$HOME/.ssh/osprey-dev.pem}"

if [ -z "$INSTANCE_IP" ]; then
    if [ -f "$REPO_ROOT/aws_instance_ip.txt" ]; then
        INSTANCE_IP=$(cat "$REPO_ROOT/aws_instance_ip.txt")
    else
        echo "Error: INSTANCE_IP not set and aws_instance_ip.txt not found"
        exit 1
    fi
fi

if [ ! -f "$KEY_FILE" ]; then
    echo "Error: Key file not found: $KEY_FILE"
    exit 1
fi

echo "=== Setting up SLURM on EC2 Instance ==="
echo "Instance IP: $INSTANCE_IP"
echo ""

# Create setup script on remote instance
cat > /tmp/setup_slurm_remote.sh << 'REMOTE_EOF'
#!/bin/bash
set -e

echo "=== SLURM Setup on EC2 ==="

# Get system info
HOSTNAME=$(hostname)
CPUS=$(nproc)
MEMORY_KB=$(grep MemTotal /proc/meminfo | awk '{print $2}')
MEMORY_MB=$((MEMORY_KB / 1024))
MEMORY_GB=$((MEMORY_MB / 1024))

echo "Hostname: $HOSTNAME"
echo "CPUs: $CPUS"
echo "Memory: ${MEMORY_GB}GB (${MEMORY_MB}MB)"

# Install EPEL repository (needed for SLURM)
echo ""
echo "Installing EPEL repository..."
# Amazon Linux 2 uses amazon-linux-extras for EPEL
if command -v amazon-linux-extras &> /dev/null; then
    echo "Using amazon-linux-extras to install EPEL..."
    sudo amazon-linux-extras install -y epel || {
        echo "amazon-linux-extras epel not available, trying manual EPEL install..."
        sudo yum install -y https://dl.fedoraproject.org/pub/epel/epel-release-latest-7.noarch.rpm || \
        sudo yum install -y https://dl.fedoraproject.org/pub/epel/epel-release-latest-8.noarch.rpm || \
        echo "Warning: Could not install EPEL via packages."
    }
else
    # Fallback: install EPEL manually
    sudo yum install -y https://dl.fedoraproject.org/pub/epel/epel-release-latest-7.noarch.rpm || \
    sudo yum install -y https://dl.fedoraproject.org/pub/epel/epel-release-latest-8.noarch.rpm || \
    echo "Warning: Could not install EPEL. Will try building SLURM from source."
fi

# Install SLURM and dependencies
echo ""
echo "Installing SLURM and dependencies..."
if sudo yum install -y slurm slurm-devel slurm-perlapi slurm-plugins slurm-slurmdbd slurm-slurmd slurm-slurmctld 2>/dev/null; then
    echo "SLURM installed from packages"
else
    echo "SLURM packages not available. Building from source..."
    # Install build dependencies
    sudo yum groupinstall -y "Development Tools"
    sudo yum install -y munge munge-devel perl-Switch perl-Thread-Queue \
        readline-devel openssl-devel pam-devel numactl numactl-devel \
        hwloc hwloc-devel lua lua-devel libibverbs libibverbs-devel \
        libnl3 libnl3-devel rrdtool rrdtool-devel mariadb mariadb-devel \
        python3 python3-devel
    
    # Download and build SLURM (using a stable version)
    SLURM_VERSION="23.02.4"
    cd /tmp
    if [ ! -f "slurm-${SLURM_VERSION}.tar.bz2" ]; then
        wget https://download.schedmd.com/slurm/slurm-${SLURM_VERSION}.tar.bz2
    fi
    tar xjf slurm-${SLURM_VERSION}.tar.bz2
    cd slurm-${SLURM_VERSION}
    
    ./configure --prefix=/usr --sysconfdir=/etc/slurm
    make -j$(nproc)
    sudo make install
    
    # Create slurm user if it doesn't exist
    if ! id -u slurm &>/dev/null; then
        sudo useradd -r -s /bin/false slurm
    fi
fi

# Create SLURM directories
echo ""
echo "Creating SLURM directories..."
sudo mkdir -p /var/spool/slurmctld
sudo mkdir -p /var/spool/slurmd
sudo mkdir -p /var/log/slurm
sudo chown -R slurm:slurm /var/spool/slurmctld
sudo chown -R slurm:slurm /var/spool/slurmd
sudo chown -R slurm:slurm /var/log/slurm

# Create SLURM configuration
echo ""
echo "Creating SLURM configuration..."
sudo tee /etc/slurm/slurm.conf > /dev/null <<SLURM_CONF_EOF
# SLURM configuration for single-node EC2 instance
ClusterName=osprey-cluster
ControlMachine=$HOSTNAME
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
SlurmctldLogFile=/var/log/slurm/slurmctld.log
SlurmdLogFile=/var/log/slurm/slurmd.log
NodeName=$HOSTNAME CPUs=$CPUS RealMemory=$MEMORY_MB State=UNKNOWN
PartitionName=compute Nodes=$HOSTNAME Default=YES MaxTime=INFINITE MaxNodes=1
ProctrackType=proctrack/linuxproc
ReturnToService=1
SlurmctldTimeout=300
SlurmdTimeout=300
InactiveLimit=0
MinJobAge=300
KillWait=30
Waittime=0
SLURM_CONF_EOF

# Backup original config if it exists
if [ -f /etc/slurm/slurm.conf.orig ]; then
    sudo cp /etc/slurm/slurm.conf.orig /etc/slurm/slurm.conf.orig.backup
fi
sudo cp /etc/slurm/slurm.conf /etc/slurm/slurm.conf.orig

# Create cgroup configuration (if needed)
if [ ! -f /etc/slurm/cgroup.conf ]; then
    sudo tee /etc/slurm/cgroup.conf > /dev/null <<CGROUP_CONF_EOF
# Cgroup configuration
CgroupAutomount=yes
ConstrainCores=no
ConstrainRAMSpace=no
Cgroup_CONF_EOF
fi

# Start SLURM services
echo ""
echo "Starting SLURM services..."
sudo systemctl enable slurmctld
sudo systemctl enable slurmd

# Check if services are already running
if sudo systemctl is-active --quiet slurmctld; then
    echo "slurmctld is already running, restarting..."
    sudo systemctl restart slurmctld
else
    echo "Starting slurmctld..."
    sudo systemctl start slurmctld
fi

if sudo systemctl is-active --quiet slurmd; then
    echo "slurmd is already running, restarting..."
    sudo systemctl restart slurmd
else
    echo "Starting slurmd..."
    sudo systemctl start slurmd
fi

# Wait a moment for services to start
sleep 2

# Verify SLURM is working
echo ""
echo "Verifying SLURM installation..."
if command -v sinfo &> /dev/null; then
    echo ""
    echo "SLURM node status:"
    sinfo
    echo ""
    echo "Testing SLURM with a simple job:"
    srun hostname
    echo ""
    echo "=== SLURM Setup Complete ==="
    echo ""
    echo "Useful commands:"
    echo "  sinfo              - Show node status"
    echo "  srun <command>     - Run a command via SLURM"
    echo "  sbatch <script>   - Submit a batch job"
    echo "  squeue            - Show job queue"
    echo "  scancel <job_id>  - Cancel a job"
    echo ""
    echo "Service management:"
    echo "  sudo systemctl status slurmctld  - Check controller status"
    echo "  sudo systemctl status slurmd      - Check node daemon status"
    echo "  sudo systemctl restart slurmctld  - Restart controller"
    echo "  sudo systemctl restart slurmd     - Restart node daemon"
else
    echo "Warning: SLURM commands not found in PATH"
    echo "You may need to log out and back in, or source /etc/profile.d/slurm.sh"
fi
REMOTE_EOF

# Copy setup script to instance
scp -i "$KEY_FILE" /tmp/setup_slurm_remote.sh ec2-user@"$INSTANCE_IP":/tmp/

# Run setup script
echo "Running SLURM setup on instance..."
ssh -i "$KEY_FILE" ec2-user@"$INSTANCE_IP" "bash /tmp/setup_slurm_remote.sh"

echo ""
echo "=== Setup Complete ==="
echo "SSH into instance to use SLURM:"
echo "  ssh -i $KEY_FILE ec2-user@$INSTANCE_IP"
echo ""
echo "Test SLURM:"
echo "  sinfo"
echo "  srun hostname"

