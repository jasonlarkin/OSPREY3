#!/bin/bash
# Create minimal test cases for each pipeline stage
# These are representative but fast enough for iteration

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
MINIMAL_DIR="$PROJECT_ROOT/minimal_test_cases"

echo "=== Creating Minimal Test Cases ==="
echo "Project root: $PROJECT_ROOT"
echo "Output directory: $MINIMAL_DIR"

mkdir -p "$MINIMAL_DIR"/{scope,montage,energy,kstar,arise}

# Stage 1: SCOPE Minimal Test
echo ""
echo "=== Creating SCOPE Minimal Test ==="
cat > "$MINIMAL_DIR/scope/test_scope_minimal.py" <<'EOF'
#!/usr/bin/env python3
"""Minimal SCOPE test case - 2RL0 with 3 amino acid types"""

import sys
from pathlib import Path

# Add CCKStar to path
sys.path.insert(0, str(Path(__file__).parent.parent.parent / "src/main/python/CCKStar"))

from Find_Doublets import SCOPE

# Minimal amino acid set (3 types instead of 22)
minimal_aas = ['VAL', 'ALA', 'LEU']

# Run SCOPE with minimal input
pdb_file = Path(__file__).parent.parent.parent / "examples/python.KStar/2RL0.min.reduce.pdb"
output_folder = Path(__file__).parent / "test_hulls"

print(f"Running minimal SCOPE on {pdb_file}")
print(f"Output: {output_folder}")
print(f"Amino acids: {minimal_aas}")

intrachain_pairs, interchain_pairs = SCOPE(
    str(pdb_file),
    str(output_folder),
    'G',  # Design chain
    minimal_aas,
    True,  # Save PDB hulls
    'L',   # Chirality
    []     # Fixed residues
)

print(f"\nFound {len(intrachain_pairs)} intra-chain pairs")
print(f"Found {len(interchain_pairs)} inter-chain contacts")
EOF

chmod +x "$MINIMAL_DIR/scope/test_scope_minimal.py"

# Stage 2: Energy Matrix Minimal Test
echo ""
echo "=== Creating Energy Matrix Minimal Test ==="
cat > "$MINIMAL_DIR/energy/test_energy_minimal.sh" <<'EOF'
#!/bin/bash
# Minimal energy matrix test - 2RL0 with 4 confs

cd "$(dirname "$0")/../../.."

./gradlew test \
    --tests "edu.duke.cs.osprey.energy.compiled.TestNativeConfEnergyCalculator.calcEnergy_native_all_2RL0_f64" \
    --no-daemon
EOF

chmod +x "$MINIMAL_DIR/energy/test_energy_minimal.sh"

# Stage 3: K* Minimal Test (Java)
echo ""
echo "=== Creating K* Minimal Test ==="
cat > "$MINIMAL_DIR/kstar/test_kstar_minimal.sh" <<'EOF'
#!/bin/bash
# Minimal K* test - single sequence, 1GUA, fast epsilon

cd "$(dirname "$0")/../../.."

# Use existing test but with single sequence focus
# Option 1: Use MARKStar tiny test (faster)
./gradlew test \
    --tests "edu.duke.cs.osprey.markstar.TestMARKStar.testMARKStarTinyEpsilon" \
    --no-daemon

# Option 2: Use 1GUA small test
# ./gradlew test \
#     --tests "edu.duke.cs.osprey.markstar.TestMARKStar.test1GUASmall" \
#     --no-daemon
EOF

chmod +x "$MINIMAL_DIR/kstar/test_kstar_minimal.sh"

# Stage 4: Python K* Minimal Test
echo ""
echo "=== Creating Python K* Minimal Test ==="
cat > "$MINIMAL_DIR/kstar/test_kstar_python_minimal.py" <<'EOF'
#!/usr/bin/env python3
"""Minimal Python K* test - single sequence, reduced precision"""

import sys
from pathlib import Path

# Add examples to path
sys.path.insert(0, str(Path(__file__).parent.parent.parent / "examples/python.KStar"))

import osprey
osprey.start()

# Minimal configuration
ffparams = osprey.ForcefieldParams()
mol = osprey.readPdb('2RL0.min.reduce.pdb')
templateLib = osprey.TemplateLibrary(ffparams.forcefld)

# Minimal protein strand (fewer residues)
protein = osprey.Strand(mol, templateLib=templateLib, residues=['G648', 'G651'])
protein.flexibility['G649'].setLibraryRotamers(osprey.WILD_TYPE, 'TYR', 'ALA').addWildTypeRotamers().setContinuous()

# Minimal ligand strand
ligand = osprey.Strand(mol, templateLib=templateLib, residues=['A155', 'A160'])
ligand.flexibility['A156'].setLibraryRotamers(osprey.WILD_TYPE).addWildTypeRotamers().setContinuous()

# Conf spaces
proteinConfSpace = osprey.ConfSpace(protein)
ligandConfSpace = osprey.ConfSpace(ligand)
complexConfSpace = osprey.ConfSpace([protein, ligand])

# Energy calculator
parallelism = osprey.Parallelism(cpuCores=2)  # Minimal cores
ecalc = osprey.EnergyCalculator(complexConfSpace, ffparams, parallelism=parallelism)

# K* with fast epsilon (less precise, faster)
kstar = osprey.KStar(
    proteinConfSpace,
    ligandConfSpace,
    complexConfSpace,
    epsilon=0.95,  # Less precise, faster convergence
    writeSequencesToConsole=True,
    writeSequencesToFile='kstar.minimal.results.tsv'
)

# Run (will process fewer sequences due to smaller conf space)
scoredSequences = kstar.run(ecalc.tasks)
print(f"\nCompleted: {len(scoredSequences)} sequences")
EOF

chmod +x "$MINIMAL_DIR/kstar/test_kstar_python_minimal.py"

# Create profiling wrapper
echo ""
echo "=== Creating Profiling Scripts ==="
cat > "$MINIMAL_DIR/profile_all_minimal.sh" <<'EOF'
#!/bin/bash
# Profile all minimal test cases

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
OUTPUT_DIR="$PROJECT_ROOT/pipeline_analysis/memory"

mkdir -p "$OUTPUT_DIR"

echo "=== Profiling Minimal Test Cases ==="

# Stage 1: SCOPE
echo ""
echo "=== Profiling SCOPE Minimal ==="
cd "$SCRIPT_DIR/scope"
if command -v valgrind &> /dev/null; then
    valgrind --tool=massif --massif-out-file="$OUTPUT_DIR/scope_minimal_massif.out" \
        python3 test_scope_minimal.py 2>&1 | tee "$OUTPUT_DIR/scope_minimal_output.txt"
else
    python3 test_scope_minimal.py 2>&1 | tee "$OUTPUT_DIR/scope_minimal_output.txt"
fi

# Stage 2: Energy Matrix
echo ""
echo "=== Profiling Energy Matrix Minimal ==="
cd "$SCRIPT_DIR/energy"
if command -v valgrind &> /dev/null; then
    valgrind --tool=massif --massif-out-file="$OUTPUT_DIR/energy_minimal_massif.out" \
        bash test_energy_minimal.sh 2>&1 | tee "$OUTPUT_DIR/energy_minimal_output.txt"
else
    bash test_energy_minimal.sh 2>&1 | tee "$OUTPUT_DIR/energy_minimal_output.txt"
fi

# Stage 3: K* Algorithm
echo ""
echo "=== Profiling K* Minimal ==="
cd "$SCRIPT_DIR/kstar"

# Java K* test
if command -v valgrind &> /dev/null; then
    export JAVA_OPTS="-Xmx2g -XX:+PrintGCDetails -XX:+PrintGCDateStamps -Xloggc:$OUTPUT_DIR/kstar_minimal_gc.log"
    valgrind --tool=massif --massif-out-file="$OUTPUT_DIR/kstar_minimal_massif.out" \
        bash test_kstar_minimal.sh 2>&1 | tee "$OUTPUT_DIR/kstar_minimal_output.txt"
else
    export JAVA_OPTS="-Xmx2g -XX:+PrintGCDetails -XX:+PrintGCDateStamps -Xloggc:$OUTPUT_DIR/kstar_minimal_gc.log"
    bash test_kstar_minimal.sh 2>&1 | tee "$OUTPUT_DIR/kstar_minimal_output.txt"
fi

# Python K* test (optional)
echo ""
echo "=== Profiling Python K* Minimal ==="
cd "$PROJECT_ROOT/examples/python.KStar"
if command -v valgrind &> /dev/null; then
    valgrind --tool=massif --massif-out-file="$OUTPUT_DIR/kstar_python_minimal_massif.out" \
        python3 "$SCRIPT_DIR/kstar/test_kstar_python_minimal.py" 2>&1 | tee "$OUTPUT_DIR/kstar_python_minimal_output.txt"
else
    python3 "$SCRIPT_DIR/kstar/test_kstar_python_minimal.py" 2>&1 | tee "$OUTPUT_DIR/kstar_python_minimal_output.txt"
fi

echo ""
echo "=== Profiling Complete ==="
echo "Results in: $OUTPUT_DIR"
echo ""
echo "Analyze with:"
echo "  python3 $PROJECT_ROOT/scripts/analyze_pipeline_memory.py \\"
echo "      --massif-file $OUTPUT_DIR/kstar_minimal_massif.out \\"
echo "      --stage kstar \\"
echo "      --output-dir $OUTPUT_DIR"
EOF

chmod +x "$MINIMAL_DIR/profile_all_minimal.sh"

echo ""
echo "=== Minimal Test Cases Created ==="
echo "Location: $MINIMAL_DIR"
echo ""
echo "Test cases:"
echo "  - SCOPE: $MINIMAL_DIR/scope/test_scope_minimal.py"
echo "  - Energy: $MINIMAL_DIR/energy/test_energy_minimal.sh"
echo "  - K* Java: $MINIMAL_DIR/kstar/test_kstar_minimal.sh"
echo "  - K* Python: $MINIMAL_DIR/kstar/test_kstar_python_minimal.py"
echo ""
echo "Profile all:"
echo "  $MINIMAL_DIR/profile_all_minimal.sh"

