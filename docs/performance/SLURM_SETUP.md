# SLURM Setup for Testing

## Overview

SLURM is needed for:
- Multi-node testing (Phase 3)
- Production deployment
- Resource management
- Testing kstar_parallel job submission

## Installation Options

### Option 1: Local SLURM (WSL-Friendly)

**Automated Setup**:
```bash
# Run the WSL-friendly setup script
./scripts/setup_slurm_local_wsl.sh
```

**Manual Steps** (if script doesn't work):
```bash
# Install SLURM
sudo apt-get update
sudo apt-get install slurm-wlm slurm-wlm-doc

# Configuration is created by setup script at /etc/slurm/slurm.conf
# Uses /tmp for spool directories (WSL-friendly, no systemd required)
```

**Starting SLURM** (without systemd):
```bash
# Start SLURM services manually
./scripts/start_slurm_local.sh

# Or manually:
sudo slurmctld -D &  # Controller daemon
sudo slurmd -D &     # Compute node daemon
```

**Stopping SLURM**:
```bash
./scripts/stop_slurm_local.sh

# Or manually:
sudo pkill slurmctld
sudo pkill slurmd
```

**Testing**:
```bash
sinfo                    # Show cluster info
srun hostname            # Run a simple job
sbatch --wrap='sleep 10' # Submit a batch job
```

**Limitations**:
- Single-node only (no network)
- Manual start/stop (no systemd integration)
- Uses /tmp for spool directories (cleared on reboot)

### Option 2: AWS with SLURM (Recommended)

**Use AWS instances with SLURM pre-configured**:
- AWS ParallelCluster
- Or manual setup on EC2 instances

**Benefits**:
- Real multi-node environment
- Production-like setup
- Can test actual scaling

### Option 3: Skip SLURM for Phase 1

**For Phase 1 (single-node, multi-thread)**:
- SLURM not required
- Can test locally without SLURM
- Add SLURM when moving to Phase 3 (multi-node)

**Testing without SLURM**:
```bash
# Just run directly
./kstar_parallel --sequences=0-24 --output=results.txt
```

## Recommendation

**Phase 1**: Skip SLURM, test locally
- Focus on thread pool implementation
- Validate correctness
- Measure single-node performance

**Phase 3**: Add SLURM for multi-node
- Set up AWS instances
- Configure SLURM
- Test multi-node scaling

## Multi-Instance Without SLURM

Even without SLURM, can test multi-instance approach:

```bash
# Terminal 1
./kstar_parallel --sequences=0-24 --output=results_0.txt

# Terminal 2
./kstar_parallel --sequences=25-49 --output=results_1.txt

# Terminal 3
./kstar_parallel --sequences=50-74 --output=results_2.txt

# Terminal 4
./kstar_parallel --sequences=75-99 --output=results_3.txt

# Merge results
cat results_*.txt > all_results.txt
```

This validates the multi-instance approach without SLURM complexity.

