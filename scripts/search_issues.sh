#!/bin/bash
# Search GitHub issues for OSPREY3 repository
# Usage: ./search_issues.sh "search terms"

REPO="donaldlab/OSPREY3"
QUERY="$1"

if [ -z "$QUERY" ]; then
    echo "Usage: $0 \"search terms\""
    echo "Example: $0 \"StackOverflowError\""
    exit 1
fi

# GitHub API endpoint for searching issues
# No authentication needed for public repos
curl -s "https://api.github.com/search/issues?q=repo:${REPO}+is:issue+${QUERY}" | \
    jq -r '.items[] | "\(.number): \(.title) [\(.state)] - \(.html_url)"' 2>/dev/null || \
    curl -s "https://api.github.com/search/issues?q=repo:${REPO}+is:issue+${QUERY}" | \
    python3 -m json.tool | grep -E '"number"|"title"|"state"|"html_url"' | head -40

