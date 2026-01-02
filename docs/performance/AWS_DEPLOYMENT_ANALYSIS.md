# AWS Deployment Analysis: SLURM + HPC Instances

## Instance Type Recommendations

### Development: 2-4 Core Instances

**Primary Options:**

| Instance Type | vCPUs | Memory | Network | Price/hr (us-east-1) | Spot/hr | Use Case |
|---------------|-------|--------|---------|---------------------|---------|----------|
| **r6i.large** | 2 | 16 GB | 10 Gbps | ~$0.126 | ~$0.04 | Development, testing |
| **r6i.xlarge** | 4 | 32 GB | 10 Gbps | ~$0.252 | ~$0.08 | Development, small workloads |
| **c6i.large** | 2 | 4 GB | 10 Gbps | ~$0.085 | ~$0.03 | Compute-focused dev |
| **c6i.xlarge** | 4 | 8 GB | 10 Gbps | ~$0.17 | ~$0.05 | Compute-focused dev |
| **m6i.large** | 2 | 8 GB | 10 Gbps | ~$0.096 | ~$0.03 | General purpose dev |
| **m6i.xlarge** | 4 | 16 GB | 10 Gbps | ~$0.192 | ~$0.06 | General purpose dev |

**Recommended for development: r6i.xlarge (4 vCPUs, 32 GB)**
- Sufficient memory for K* testing
- 4 cores for parallel development
- $0.252/hr on-demand, ~$0.08/hr spot

### Production: 16-32 Core Instances

**Primary Options:**

| Instance Type | vCPUs | Memory | Network | Price/hr (us-east-1) | Spot/hr | Use Case |
|---------------|-------|--------|---------|---------------------|---------|----------|
| **c6i.4xlarge** | 16 | 32 GB | 12.5 Gbps | ~$0.68 | ~$0.20 | Compute-optimized, balanced |
| **c6i.8xlarge** | 32 | 64 GB | 25 Gbps | ~$1.36 | ~$0.41 | Compute-optimized, high core |
| **r6i.4xlarge** | 16 | 128 GB | 12.5 Gbps | ~$1.01 | ~$0.30 | Memory-optimized, K* workloads |
| **r6i.8xlarge** | 32 | 256 GB | 25 Gbps | ~$2.02 | ~$0.61 | Memory-optimized, large K* |
| **m6i.4xlarge** | 16 | 64 GB | 12.5 Gbps | ~$0.77 | ~$0.23 | General purpose, balanced |
| **m6i.8xlarge** | 32 | 128 GB | 25 Gbps | ~$1.54 | ~$0.46 | General purpose, high core |

### Recommended: r6i.4xlarge or r6i.8xlarge

**Rationale:**
- **K* is memory-intensive** (1.2GB+ per run, GC pressure)
- **Memory-optimized instances** (r6i) have:
  - 2x memory per vCPU vs. compute-optimized (c6i)
  - Better memory bandwidth
  - Lower GC overhead with more headroom

**For 4-8 cores**: `r6i.4xlarge` (16 vCPUs, 128 GB)
**For 16-32 cores**: `r6i.8xlarge` (32 vCPUs, 256 GB)

## Cost Estimation

### Single Instance Costs (us-east-1, on-demand)

| Instance | vCPUs | Memory | On-Demand/hr | Spot/hr (70% discount) |
|----------|-------|--------|--------------|----------------------|
| r6i.4xlarge | 16 | 128 GB | $1.01 | ~$0.30 |
| r6i.8xlarge | 32 | 256 GB | $2.02 | ~$0.61 |

### Multi-Node Cluster Costs

**4-node cluster (r6i.4xlarge):**
- On-demand: 4 × $1.01 = **$4.04/hr**
- Spot: 4 × $0.30 = **$1.20/hr**

**4-node cluster (r6i.8xlarge):**
- On-demand: 4 × $2.02 = **$8.08/hr**
- Spot: 4 × $0.61 = **$2.44/hr**

### Typical K* Run Costs

**Current performance** (1-2 days sequential):
- Single r6i.4xlarge: 24-48 hours × $1.01 = **$24-48**
- Single r6i.8xlarge: 24-48 hours × $2.02 = **$48-96**

**Optimized performance** (10-20 minutes parallel):
- Single r6i.4xlarge: 0.33 hours × $1.01 = **$0.33**
- Single r6i.8xlarge: 0.33 hours × $2.02 = **$0.67**
- 4-node r6i.4xlarge: 0.33 hours × $4.04 = **$1.33**
- 4-node r6i.8xlarge: 0.33 hours × $8.08 = **$2.67**

**Cost reduction**: **72-144x** (from $24-48 to $0.33-2.67)

## SLURM Configuration

### Recommended Setup

**Cluster Configuration:**
- **Head node**: t3.medium (2 vCPU, 4 GB) - SLURM controller
- **Compute nodes**: r6i.4xlarge or r6i.8xlarge
- **Storage**: EBS gp3 (for input/output files)
- **Network**: VPC with enhanced networking

### SLURM Job Script Example

```bash
#!/bin/bash
#SBATCH --job-name=kstar_parallel
#SBATCH --nodes=4
#SBATCH --ntasks-per-node=1
#SBATCH --cpus-per-task=32
#SBATCH --mem=256G
#SBATCH --time=01:00:00
#SBATCH --partition=compute

# Load modules
module load gcc/11.2.0
module load openmpi/4.1.0

# Set environment
export OMP_NUM_THREADS=32
export OSPREY_NUM_THREADS=32

# Run K* with MPI
mpirun -np 4 ./kstar_mpi \
    --confspace complex.ccsx \
    --sequences sequences.txt \
    --output results.tsv
```

### Auto-Scaling Configuration

**AWS ParallelCluster / SLURM:**

```yaml
Scheduling:
  Scheduler: slurm
  SlurmQueues:
    - Name: compute
      ComputeResources:
        - Name: r6i-4xlarge
          InstanceType: r6i.4xlarge
          MinCount: 0
          MaxCount: 10
          SpotPrice: 0.30
        - Name: r6i-8xlarge
          InstanceType: r6i.8xlarge
          MinCount: 0
          MaxCount: 5
          SpotPrice: 0.61
```

## Cost Optimization Strategies

### 1. Use Spot Instances

**Savings**: 70% discount
- **Risk**: Can be interrupted (but K* can checkpoint)
- **Recommendation**: Use for non-critical runs

### 2. Reserved Instances (1-year)

**Savings**: 30-40% discount
- **Cost**: r6i.4xlarge: ~$0.60/hr (vs. $1.01 on-demand)
- **Recommendation**: For predictable workloads

### 3. Savings Plans (Flexible)

**Savings**: 20-30% discount
- **Flexibility**: Can switch instance types
- **Recommendation**: For variable workloads

### 4. Auto-Scaling

**Strategy**: Scale down when idle
- **Head node**: Always on (t3.medium: $0.04/hr)
- **Compute nodes**: Scale to zero when idle
- **Savings**: Pay only for compute time

## Performance vs. Cost Trade-offs

### Option 1: Single r6i.4xlarge (16 cores)

**Performance:**
- 16x parallel speedup
- 1-2 days → **1.5-3 hours**

**Cost:**
- On-demand: $1.01/hr × 2 hours = **$2.02**
- Spot: $0.30/hr × 2 hours = **$0.60**

**Best for**: Development, testing, small workloads

### Option 2: Single r6i.8xlarge (32 cores)

**Performance:**
- 32x parallel speedup
- 1-2 days → **45-90 minutes**

**Cost:**
- On-demand: $2.02/hr × 1.5 hours = **$3.03**
- Spot: $0.61/hr × 1.5 hours = **$0.92**

**Best for**: Medium workloads, production

### Option 3: 4-node r6i.4xlarge (64 cores total)

**Performance:**
- 64x parallel speedup (with MPI)
- 1-2 days → **20-40 minutes**

**Cost:**
- On-demand: $4.04/hr × 0.5 hours = **$2.02**
- Spot: $1.20/hr × 0.5 hours = **$0.60**

**Best for**: Large workloads, time-critical

### Option 4: 4-node r6i.8xlarge (128 cores total)

**Performance:**
- 128x parallel speedup (with MPI)
- 1-2 days → **10-20 minutes**

**Cost:**
- On-demand: $8.08/hr × 0.33 hours = **$2.67**
- Spot: $2.44/hr × 0.33 hours = **$0.81**

**Best for**: Very large workloads, maximum speed

## Recommendations

### Development/Testing (2-4 cores)
- **Instance**: r6i.xlarge (4 vCPUs, 32 GB)
- **Cost**: $0.08/hr (spot) or $0.252/hr (on-demand)
- **Use case**: Code development, unit testing, small K* runs
- **Monthly cost** (8hr/day, spot): ~$19/month
- **Monthly cost** (8hr/day, on-demand): ~$60/month

### Scaling Tests (16-32 cores)
- **Instance**: r6i.4xlarge (16 cores) or r6i.8xlarge (32 cores)
- **Cost**: $0.30-0.61/hr (spot) or $1.01-2.02/hr (on-demand)
- **Use case**: Performance validation, production-like workloads
- **Runtime**: 1.5-3 hours (16 cores) or 45-90 minutes (32 cores)

### Production (Medium Workloads)
- **Instance**: r6i.8xlarge (32 cores)
- **Cost**: ~$0.61-2.02/hr (spot/on-demand)
- **Runtime**: 45-90 minutes

### Production (Large Workloads)
- **Cluster**: 4× r6i.4xlarge (64 cores)
- **Cost**: ~$0.60-2.02 per run (spot/on-demand)
- **Runtime**: 20-40 minutes

### Maximum Performance
- **Cluster**: 4× r6i.8xlarge (128 cores)
- **Cost**: ~$0.81-2.67 per run (spot/on-demand)
- **Runtime**: 10-20 minutes

## Total Cost of Ownership

### Development Costs (r6i.xlarge, 4 vCPUs)

**Daily development** (8 hours/day):
- Spot: $0.08/hr × 8 = $0.64/day = **$19/month**
- On-demand: $0.252/hr × 8 = $2.02/day = **$60/month**

**Plus fixed costs:**
- Head node (t3.medium): $0.04/hr × 24 × 30 = **$29/month**
- EBS storage: ~$10-20/month (development volumes)
- **Total development**: ~$58-109/month

### Production Costs (assuming 10 runs/day)

| Configuration | Runs/day | Cost/run | Daily | Monthly |
|---------------|----------|----------|-------|---------|
| r6i.4xlarge (spot) | 10 | $0.60 | $6.00 | $180 |
| r6i.8xlarge (spot) | 10 | $0.92 | $9.20 | $276 |
| 4× r6i.4xlarge (spot) | 10 | $0.60 | $6.00 | $180 |
| 4× r6i.8xlarge (spot) | 10 | $0.81 | $8.10 | $243 |

**Plus fixed costs:**
- Head node (t3.medium): $0.04/hr × 24 × 30 = **$29/month**
- EBS storage: ~$10-50/month (depending on size)
- **Total production**: ~$200-350/month for 10 runs/day

## Conclusion

**Development environment:**
- **Instance**: r6i.xlarge (4 vCPUs, 32 GB)
- **Pricing**: Spot instances ($0.08/hr)
- **Monthly cost**: ~$58/month (8hr/day development)
- **Use for**: Code development, unit testing, small K* validation

**Scaling tests:**
- **Instance**: r6i.4xlarge (16 cores) or r6i.8xlarge (32 cores)
- **Pricing**: Spot instances ($0.30-0.61/hr)
- **Use for**: Performance validation, production-like workloads
- **Cost**: ~$0.60-0.92 per test run

**Production:**
- **Instance**: r6i.8xlarge (32 cores) or 4-node cluster
- **Pricing**: Spot instances ($0.61/hr per node)
- **Expected cost**: ~$0.92 per K* run (single node) or ~$0.81 per run (4-node)
- **Expected runtime**: 45-90 minutes (single) or 10-20 minutes (4-node)

