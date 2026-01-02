#!/usr/bin/env bash
# Shared environment helpers for scripts/ (portable across WSL/Linux/macOS)
#
# Conventions:
# - REPO_ROOT should be set by caller (recommended).
# - External tooling venv is OPTIONAL. If present, we activate it.
# - Prefer environment variable overrides over hardcoded absolute paths.
#
# Supported env vars:
# - OSPREY_TOOLS_VENV: path to a python venv directory to activate
#
# Fallback search (if OSPREY_TOOLS_VENV not set):
# - $REPO_ROOT/.venv
# - $REPO_ROOT/../tools/.venv
# - $REPO_ROOT/../osprey_tools/.venv

set -euo pipefail

osprey_repo_root_from_script_dir() {
  local script_dir="$1"
  (cd "$script_dir/.." && pwd)
}

osprey_find_tools_venv() {
  local repo_root="$1"

  if [[ -n "${OSPREY_TOOLS_VENV:-}" && -d "${OSPREY_TOOLS_VENV}" ]]; then
    echo "${OSPREY_TOOLS_VENV}"
    return 0
  fi

  local candidates=(
    "${repo_root}/.venv"
    "${repo_root}/../tools/.venv"
    "${repo_root}/../osprey_tools/.venv"
  )

  local c
  for c in "${candidates[@]}"; do
    if [[ -d "$c" ]]; then
      echo "$c"
      return 0
    fi
  done

  echo ""
}

osprey_try_activate_tools_venv() {
  local repo_root="$1"
  local venv_path
  venv_path="$(osprey_find_tools_venv "$repo_root")"

  if [[ -n "$venv_path" && -f "$venv_path/bin/activate" ]]; then
    # shellcheck disable=SC1090
    source "$venv_path/bin/activate"
    export OSPREY_TOOLS_VENV="$venv_path"
    return 0
  fi

  return 1
}

