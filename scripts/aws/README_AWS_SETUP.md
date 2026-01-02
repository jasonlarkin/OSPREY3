# AWS Instance Setup for OSPREY

## Overview

This directory contains scripts to provision and configure AWS EC2 instances for OSPREY development and testing.

## Prerequisites

1. AWS account with appropriate permissions
2. AWS CLI installed and configured
3. SSH key pair for instance access

## Quick Start

### 1. Setup AWS CLI

```bash
./scripts/aws/setup_aws_cli.sh
```

This will:
- Check/install AWS CLI
- Configure credentials from `aws_keys.md`
- Set default region to `us-east-1`

### 2. Provision Development Instance

```bash
./scripts/aws/provision_dev_instance.sh
```

This will:
- Create key pair (`osprey-dev.pem`)
- Create security group
- Launch `r6i.xlarge` instance (4 vCPUs, 32 GB)
- Save instance ID and IP to files

**Configuration:**
- Instance type: `r6i.xlarge` (4 vCPUs, 32 GB) - $0.252/hr on-demand, ~$0.08/hr spot
- AMI: Amazon Linux 2023
- Region: us-east-1

**To use spot instances:**
```bash
INSTANCE_TYPE=r6i.xlarge USE_SPOT=true ./scripts/aws/provision_dev_instance.sh
```

### 3. Setup OSPREY on Instance

```bash
./scripts/aws/setup_osprey_on_instance.sh
```

This will:
- Wait for instance to be ready
- Install OSPREY dependencies
- Setup Python virtual environment
- Configure Java environment

### 4. Deploy OSPREY Code

**Option 1: Clone from repository**
```bash
ssh -i osprey-dev.pem ec2-user@<INSTANCE_IP>
cd /home/ec2-user/osprey-fork_modern
git clone <your-repo-url> .
```

**Option 2: Upload via rsync**
```bash
rsync -avz -e "ssh -i osprey-dev.pem" \
    --exclude '.git' \
    --exclude 'build' \
    --exclude '*.pyc' \
    ./ ec2-user@<INSTANCE_IP>:/home/ec2-user/osprey-fork_modern/
```

### 5. Build and Test

```bash
ssh -i osprey-dev.pem ec2-user@<INSTANCE_IP>
cd /home/ec2-user/osprey-fork_modern
./gradlew build
export OSPREY_TOOLS_VENV="/home/ec2-user/osprey_tools_venv"
source "$OSPREY_TOOLS_VENV/bin/activate"
python3 -c "import osprey; osprey.start()"
```

## SIMD Benchmarking / Profiling (ConfEcalc)

For native Linux profiling (avoid WSL noise), run the ConfEcalc reproducibility sweep on the instance:

```bash
# local machine (this repo)
INSTANCE_IP=$(cat aws_instance_ip.txt)
KEY_FILE=~/.ssh/osprey-dev.pem
./scripts/aws/run_simd_repro_on_instance.sh
```

Overrides (optional):

```bash
ATOMS=500 AMBER=1000 EEF1=500 ./scripts/aws/run_simd_repro_on_instance.sh
PIN_CORE=0 BENCH_REPS=30 ./scripts/aws/run_simd_repro_on_instance.sh
```

Outputs are written on the instance under `perf_results/repro_*` and include:
- `repro_summary.csv`
- `plots/*.png` (if matplotlib is available)

## Instance Types

### Development (Recommended)
- **r6i.xlarge**: 4 vCPUs, 32 GB - $0.252/hr on-demand, ~$0.08/hr spot
- Good for: Development, testing, small K* runs

### Scaling Tests
- **r6i.4xlarge**: 16 vCPUs, 128 GB - $1.01/hr on-demand, ~$0.30/hr spot
- Good for: Performance validation, medium workloads

- **r6i.8xlarge**: 32 vCPUs, 256 GB - $2.02/hr on-demand, ~$0.61/hr spot
- Good for: Large workloads, production-like testing

## Cost Management

### Spot Instances
Use spot instances for development to save 70%:
```bash
USE_SPOT=true ./scripts/aws/provision_dev_instance.sh
```

### Instance Management
```bash
# Stop instance (saves compute costs, keeps storage)
aws ec2 stop-instances --instance-ids <INSTANCE_ID>

# Start instance
aws ec2 start-instances --instance-ids <INSTANCE_ID>

# Terminate instance (deletes everything)
aws ec2 terminate-instances --instance-ids <INSTANCE_ID>
```

## SLURM Setup

For multi-node SLURM clusters, see:
- `docs/performance/AWS_DEPLOYMENT_ANALYSIS.md` - Architecture and configuration
- `scripts/setup_slurm_local_wsl.sh` - Local SLURM setup (reference)

## Security Notes

1. **Key file security**: Keep `osprey-dev.pem` secure, never commit to git
2. **Security group**: Currently allows SSH from anywhere (0.0.0.0/0)
   - For production, restrict to your IP
3. **Credentials**: AWS credentials in `aws_keys.md` are temporary (session tokens)
   - Refresh as needed from AWS console

## Troubleshooting

### SSH Connection Issues
```bash
# Check instance status
aws ec2 describe-instance-status --instance-ids <INSTANCE_ID>

# Check security group
aws ec2 describe-security-groups --group-names osprey-dev-sg
```

### Instance Setup Issues
```bash
# Check setup logs on instance
ssh -i osprey-dev.pem ec2-user@<INSTANCE_IP> 'cat /var/log/osprey-setup.log'

# Check system logs
ssh -i osprey-dev.pem ec2-user@<INSTANCE_IP> 'sudo journalctl -u cloud-init'
```

## Next Steps

After instance is set up:
1. Deploy OSPREY code
2. Build OSPREY
3. Test K* parallelization
4. Setup SLURM for multi-node testing
5. Run LEaP batching/caching optimizations

