#!/usr/bin/env bash
# Run ConfEcalc SIMD reproducibility sweep on an AWS instance.
#
# Designed for Amazon Linux 2 (AL2), where the default GCC is too old for C++20.
# Installs/uses gcc10 toolchain (binaries are gcc10-gcc / gcc10-g++) and cmake3.
#
# What it does on the instance:
# - installs: perf, cmake3, gcc10-c++, make, python3, git
# - ensures `cmake` points to cmake3
# - builds ConfEcalc benchmarks with CC/CXX set to gcc10
# - runs: scripts/tools/repro_simd_wsl.sh
# - runs: scripts/tools/plot_repro_simd_results.py on the latest repro dir
#
# Usage:
#   ./scripts/aws/run_simd_repro_on_instance.sh
#
# Config via env:
#   INSTANCE_IP=54.x.x.x
#   KEY_FILE=~/.ssh/osprey-dev.pem
#   REPO_DIR=/home/ec2-user/osprey-fork_modern
#
# You can also override sweep params passed through env (default values come from repro_simd_wsl.sh):
#   ATOMS=200 AMBER=2000 EEF1=1000 BENCH_ITERS=20000 BENCH_REPS=15 PERF_ITERS=2000 PERF_REPS=10 PIN_CORE=0
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

INSTANCE_IP="${INSTANCE_IP:-}"
KEY_FILE="${KEY_FILE:-$HOME/.ssh/osprey-dev.pem}"
REPO_DIR="${REPO_DIR:-/home/ec2-user/osprey-fork_modern}"

if [ -z "$INSTANCE_IP" ]; then
  if [ -f "$REPO_ROOT/aws_instance_ip.txt" ]; then
    INSTANCE_IP="$(cat "$REPO_ROOT/aws_instance_ip.txt")"
  else
    echo "ERROR: INSTANCE_IP not set and aws_instance_ip.txt not found" >&2
    exit 1
  fi
fi

if [ ! -f "$KEY_FILE" ]; then
  echo "ERROR: Key file not found: $KEY_FILE" >&2
  exit 1
fi

echo "=== Run SIMD reproducibility on AWS ==="
echo "Instance: $INSTANCE_IP"
echo "Repo dir: $REPO_DIR"
echo ""

# Pass through sweep env vars explicitly (only if set) so users can override.
PASS_ENV=()
for v in ATOMS AMBER EEF1 BENCH_ITERS BENCH_REPS DIRECT_ITERS DIRECT_REPS PERF_ITERS PERF_REPS PIN_CORE; do
  if [ -n "${!v-}" ]; then
    PASS_ENV+=("$v=${!v}")
  fi
done

ssh -o StrictHostKeyChecking=no -i "$KEY_FILE" "ec2-user@$INSTANCE_IP" bash -lc "'
set -euo pipefail

echo \"== OS ==\"
cat /etc/os-release || true

cd \"$REPO_DIR\"

echo \"== install deps (AL2 uses yum) ==\"
sudo yum install -y perf cmake3 gcc10 gcc10-c++ make python3 git >/dev/null

# Ensure cmake is available as `cmake`
if ! command -v cmake >/dev/null 2>&1; then
  sudo ln -sf /usr/bin/cmake3 /usr/local/bin/cmake
fi

echo \"== tool versions ==\"
perf --version || true
cmake --version | head -n 1 || true
/usr/bin/gcc10-g++ --version | head -n 1 || true
python3 --version || true

# Build with gcc10 toolchain (C++20)
echo \"== build ConfEcalc (gcc10) ==\"
cd src/main/cc/ConfEcalc
rm -rf build
CC=/usr/bin/gcc10-gcc CXX=/usr/bin/gcc10-g++ cmake -B build -DENABLE_SIMD=ON -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build build --target ConfEcalc benchmark_scalar_only benchmark_avx2_only benchmark_avx512_only benchmark_simd_direct -j \"\$(nproc)\" >/dev/null
cd \"$REPO_DIR\"

# Optional plotting deps. If you're in a venv, do NOT use --user.
python3 -m pip install -q matplotlib || true

echo \"== run repro ==\"
${PASS_ENV[*]} bash scripts/tools/repro_simd_wsl.sh

LATEST=\$(ls -1dt perf_results/repro_* | head -n 1)
echo \"LATEST=\$LATEST\"
python3 scripts/tools/plot_repro_simd_results.py \"\$LATEST\" || true

echo \"== summary head ==\"
head -n 40 \"\$LATEST/repro_summary.csv\" || true

echo \"== plots ==\"
ls -lah \"\$LATEST/plots\" || true
'"

