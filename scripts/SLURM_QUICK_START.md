# SLURM Quick Start Guide

> ✅ **Setup Complete!** See `SLURM_SETUP_COMPLETE.md` for verification and next steps.

## Installation (WSL/Linux)

```bash
# 1. Install and configure SLURM
./scripts/setup_slurm_local_wsl.sh

# 2. Start SLURM services
./scripts/start_slurm_local.sh

# 3. Verify SLURM is running
sinfo
```

## Basic Testing

```bash
# Test SLURM is working
srun hostname

# Submit a simple batch job
sbatch --wrap='echo "Hello from SLURM"'

# Check job queue
squeue

# View job output
cat slurm-*.out
```

## Testing kstar_parallel with SLURM

```bash
# Set parameters (optional)
export NUM_SEQUENCES=10
export NUM_THREADS=4
export CONFSPACE=/path/to/confspace.ccsx
export OUTPUT_DIR=./slurm_test_results

# Run test
./scripts/test_kstar_parallel_slurm.sh
```

## Stopping SLURM

```bash
./scripts/stop_slurm_local.sh
```

## Troubleshooting

**SLURM services won't start:**
```bash
# Check if ports are in use
sudo lsof -i :6817  # slurmctld
sudo lsof -i :6818  # slurmd

# Check configuration
sudo slurmd -C

# View logs
tail -f /tmp/slurmctld.log
tail -f /tmp/slurmd.log
```

**Jobs stuck in PENDING:**
```bash
# Check node state
sinfo
scontrol show node

# Restart services
./scripts/stop_slurm_local.sh
./scripts/start_slurm_local.sh
```

**Permission errors:**
```bash
# Make sure /tmp/slurm* directories are writable
sudo chmod 777 /tmp/slurmctld /tmp/slurmd
```

