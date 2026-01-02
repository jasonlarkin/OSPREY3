#!/usr/bin/env bash
set -euo pipefail

# Thin wrapper to run the OSPREY CLI from this repo checkout.
# Prefer the self-contained image build (it includes its own `java`),
# since `build/install/osprey/bin/osprey` may not have a bundled JRE.

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
osprey_bin="${repo_root}/build/image/bin/osprey"

if [[ ! -x "${osprey_bin}" ]]; then
  echo "ERROR: expected OSPREY CLI at: ${osprey_bin}" >&2
  echo "Try building it first (from repo root): ./gradlew installDist" >&2
  exit 1
fi

exec "${osprey_bin}" "$@"


