#!/bin/bash
# Test kstar_parallel with SLURM job submission
# Assumes kstar_parallel is built and SLURM is running

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$REPO_ROOT"

# Default values
NUM_SEQUENCES="${NUM_SEQUENCES:-10}"
NUM_THREADS="${NUM_THREADS:-4}"
CONFSPACE="${CONFSPACE:-}"
OUTPUT_DIR="${OUTPUT_DIR:-./slurm_test_results}"

echo "=== Testing kstar_parallel with SLURM ==="
echo "NUM_SEQUENCES: $NUM_SEQUENCES"
echo "NUM_THREADS: $NUM_THREADS"
echo "OUTPUT_DIR: $OUTPUT_DIR"
echo ""

# Check if SLURM is running
if ! command -v sinfo &> /dev/null; then
    echo "Error: SLURM not installed"
    echo "Run: ./scripts/setup_slurm_local_wsl.sh"
    exit 1
fi

if ! sinfo &> /dev/null; then
    echo "Error: SLURM services not running"
    echo "Start with: ./scripts/start_slurm_local.sh"
    exit 1
fi

# Check if kstar_parallel exists
KSTAR_PARALLEL="$REPO_ROOT/build/image/bin/kstar_parallel"
if [ ! -f "$KSTAR_PARALLEL" ]; then
    echo "Error: kstar_parallel not found at $KSTAR_PARALLEL"
    echo "Build with: ./gradlew runtime"
    exit 1
fi

# Create output directory
mkdir -p "$OUTPUT_DIR"

# Create SLURM batch script
BATCH_SCRIPT="$OUTPUT_DIR/kstar_test.slurm"
cat > "$BATCH_SCRIPT" <<EOF
#!/bin/bash
#SBATCH --job-name=kstar_parallel_test
#SBATCH --output=$OUTPUT_DIR/kstar_%j.out
#SBATCH --error=$OUTPUT_DIR/kstar_%j.err
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=$NUM_THREADS
#SBATCH --mem=4G
#SBATCH --time=30:00

# Set environment
export OSPREY_MINIMIZE_CCD_OMP=$NUM_THREADS

# Run kstar_parallel
cd "$REPO_ROOT"
$KSTAR_PARALLEL \\
    --confspace="$CONFSPACE" \\
    --sequences=0-$((NUM_SEQUENCES-1)) \\
    --num-threads=$NUM_THREADS \\
    --output="$OUTPUT_DIR/results.txt"

echo "Job completed at \$(date)"
EOF

echo "Submitting SLURM job..."
echo "Batch script: $BATCH_SCRIPT"
echo ""

# Submit job
JOB_ID=$(sbatch "$BATCH_SCRIPT" | awk '{print $4}')
echo "Job submitted with ID: $JOB_ID"
echo ""

# Show job status
echo "Job status:"
squeue -j "$JOB_ID"
echo ""

echo "Monitor job with:"
echo "  squeue -j $JOB_ID"
echo "  tail -f $OUTPUT_DIR/kstar_${JOB_ID}.out"
echo "  tail -f $OUTPUT_DIR/kstar_${JOB_ID}.err"
echo ""
echo "Cancel job with:"
echo "  scancel $JOB_ID"
echo ""

