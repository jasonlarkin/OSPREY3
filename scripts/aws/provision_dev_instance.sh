#!/bin/bash
# Provision a development EC2 instance for OSPREY

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Configuration
INSTANCE_TYPE="${INSTANCE_TYPE:-r6i.xlarge}"  # 4 vCPUs, 32 GB
AMI_ID="${AMI_ID:-ami-0c02fb55956c7d316}"      # Amazon Linux 2023 (us-east-1)
KEY_NAME="${KEY_NAME:-osprey-dev}"
SECURITY_GROUP="${SECURITY_GROUP:-osprey-dev-sg}"
REGION="${AWS_REGION:-us-east-1}"
PROFILE="${AWS_PROFILE:-205930609292_AdministratorAccess}"
USE_SPOT="${USE_SPOT:-false}"  # Use on-demand by default for stateful instances
EBS_SIZE="${EBS_SIZE:-100}"  # GB for persistent storage

export AWS_PROFILE="$PROFILE"
export AWS_DEFAULT_REGION="$REGION"

echo "=== Provisioning OSPREY Development Instance ==="
echo "Instance type: $INSTANCE_TYPE"
echo "Region: $REGION"
echo "Profile: $PROFILE"
echo ""

# Check AWS CLI
if ! command -v aws &> /dev/null; then
    echo "Error: AWS CLI not found. Run ./scripts/aws/setup_aws_cli.sh first"
    exit 1
fi

# Check credentials
if ! aws sts get-caller-identity &> /dev/null; then
    echo "Error: AWS credentials not configured. Run ./scripts/aws/setup_aws_cli.sh first"
    exit 1
fi

# Create key pair if it doesn't exist
echo "Checking key pair: $KEY_NAME"
if ! aws ec2 describe-key-pairs --key-names "$KEY_NAME" &> /dev/null; then
    echo "Creating key pair: $KEY_NAME"
    aws ec2 create-key-pair --key-name "$KEY_NAME" --query 'KeyMaterial' --output text > "$REPO_ROOT/$KEY_NAME.pem"
    chmod 400 "$REPO_ROOT/$KEY_NAME.pem"
    echo "Key pair saved to: $REPO_ROOT/$KEY_NAME.pem"
    echo "WARNING: Keep this file secure! It's needed to SSH into the instance."
else
    echo "Key pair exists"
fi

# Create security group if it doesn't exist
echo "Checking security group: $SECURITY_GROUP"
SG_ID=$(aws ec2 describe-security-groups --group-names "$SECURITY_GROUP" --query 'SecurityGroups[0].GroupId' --output text 2>/dev/null || echo "")

if [ -z "$SG_ID" ] || [ "$SG_ID" == "None" ]; then
    echo "Creating security group: $SECURITY_GROUP"
    SG_ID=$(aws ec2 create-security-group \
        --group-name "$SECURITY_GROUP" \
        --description "OSPREY development instance security group" \
        --query 'GroupId' --output text)
    
    # Allow SSH from anywhere (for development)
    aws ec2 authorize-security-group-ingress \
        --group-id "$SG_ID" \
        --protocol tcp \
        --port 22 \
        --cidr 0.0.0.0/0
    
    echo "Security group created: $SG_ID"
else
    echo "Security group exists: $SG_ID"
fi

# User data script for instance setup
USER_DATA=$(cat << 'EOF'
#!/bin/bash
# OSPREY development instance setup

# Update system
yum update -y

# Install development tools
yum groupinstall -y "Development Tools"
yum install -y \
    python3 python3-pip \
    java-17-amazon-corretto-devel \
    git \
    wget \
    curl \
    htop \
    vim

# Install SLURM dependencies
yum install -y \
    munge \
    munge-devel \
    perl-Switch \
    perl-Thread-Queue \
    readline-devel \
    openssl-devel \
    pam-devel \
    numactl \
    numactl-devel \
    hwloc \
    hwloc-devel \
    lua \
    lua-devel \
    libibverbs \
    libibverbs-devel \
    librdmacm \
    librdmacm-devel

# Create osprey user
useradd -m -s /bin/bash osprey
mkdir -p /home/osprey
chown osprey:osprey /home/osprey

# Setup Python environment
python3 -m pip install --upgrade pip
python3 -m pip install uv

# Log setup completion
echo "OSPREY development instance setup complete" > /var/log/osprey-setup.log
date >> /var/log/osprey-setup.log
EOF
)

# Launch instance
echo ""
echo "Launching EC2 instance..."
INSTANCE_ID=$(aws ec2 run-instances \
    --image-id "$AMI_ID" \
    --instance-type "$INSTANCE_TYPE" \
    --key-name "$KEY_NAME" \
    --security-group-ids "$SG_ID" \
    --user-data "$USER_DATA" \
    --tag-specifications "ResourceType=instance,Tags=[{Key=Name,Value=osprey-dev},{Key=Project,Value=osprey},{Key=Environment,Value=dev}]" \
    --query 'Instances[0].InstanceId' \
    --output text)

echo "Instance launched: $INSTANCE_ID"
echo "Waiting for instance to be running..."

# Wait for instance to be running
aws ec2 wait instance-running --instance-ids "$INSTANCE_ID"

# Wait for instance to be running
aws ec2 wait instance-running --instance-ids "$INSTANCE_ID"

# Get instance details
INSTANCE_INFO=$(aws ec2 describe-instances --instance-ids "$INSTANCE_ID" --query 'Reservations[0].Instances[0]')
PUBLIC_IP=$(echo "$INSTANCE_INFO" | jq -r '.PublicIpAddress')
PRIVATE_IP=$(echo "$INSTANCE_INFO" | jq -r '.PrivateIpAddress')
AZ=$(echo "$INSTANCE_INFO" | jq -r '.Placement.AvailabilityZone')

# Attach EBS volume if it exists and isn't already attached
if [ -n "$EXISTING_VOLUME" ] && [ "$EXISTING_VOLUME" != "None" ]; then
    echo "Attaching EBS volume $EXISTING_VOLUME to instance..."
    # Wait a bit for instance to be fully ready
    sleep 5
    aws ec2 attach-volume \
        --volume-id "$EXISTING_VOLUME" \
        --instance-id "$INSTANCE_ID" \
        --device /dev/sdf
    
    echo "EBS volume attached. After SSH, mount it with:"
    echo "  sudo mkfs -t xfs /dev/nvme1n1  # First time only"
    echo "  sudo mkdir -p /mnt/osprey-data"
    echo "  sudo mount /dev/nvme1n1 /mnt/osprey-data"
    echo "  sudo chown ec2-user:ec2-user /mnt/osprey-data"
    echo "  echo '/dev/nvme1n1 /mnt/osprey-data xfs defaults,nofail 0 2' | sudo tee -a /etc/fstab"
fi

echo ""
echo "=== Instance Ready ==="
echo "Instance ID: $INSTANCE_ID"
echo "Public IP: $PUBLIC_IP"
echo "Private IP: $PRIVATE_IP"
echo "Availability Zone: $AZ"
if [ "$USE_SPOT" == "true" ]; then
    echo "Instance Type: SPOT (can be interrupted)"
else
    echo "Instance Type: ON-DEMAND (can be stopped/started, maintains state)"
fi
echo ""
echo "SSH command:"
echo "  ssh -i $REPO_ROOT/$KEY_NAME.pem ec2-user@$PUBLIC_IP"
echo ""
echo "Instance Management:"
echo "  Stop:  aws ec2 stop-instances --instance-ids $INSTANCE_ID"
echo "  Start: aws ec2 start-instances --instance-ids $INSTANCE_ID"
echo "  State: aws ec2 describe-instances --instance-ids $INSTANCE_ID --query 'Reservations[0].Instances[0].State.Name'"
echo ""
echo "Note: Instance setup (package installation) may take 5-10 minutes."
echo "Check setup progress:"
echo "  ssh -i $REPO_ROOT/$KEY_NAME.pem ec2-user@$PUBLIC_IP 'tail -f /var/log/osprey-setup.log'"
echo ""
echo "Save this information:"
echo "  echo '$INSTANCE_ID' > $REPO_ROOT/aws_instance_id.txt"
echo "  echo '$PUBLIC_IP' > $REPO_ROOT/aws_instance_ip.txt"

# Save instance info
echo "$INSTANCE_ID" > "$REPO_ROOT/aws_instance_id.txt"
echo "$PUBLIC_IP" > "$REPO_ROOT/aws_instance_ip.txt"
echo "$EXISTING_VOLUME" > "$REPO_ROOT/aws_volume_id.txt" 2>/dev/null || true

echo ""
echo "=== Provisioning Complete ==="

