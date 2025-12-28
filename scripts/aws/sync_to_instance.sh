#!/bin/bash
# Sync local changes to AWS instance (add, commit, push, pull on instance)

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Configuration
INSTANCE_IP="${INSTANCE_IP:-}"
KEY_FILE="${KEY_FILE:-$HOME/.ssh/osprey-dev.pem}"
BRANCH="${BRANCH:-develop}"

if [ -z "$INSTANCE_IP" ]; then
    if [ -f "$REPO_ROOT/aws_instance_ip.txt" ]; then
        INSTANCE_IP=$(cat "$REPO_ROOT/aws_instance_ip.txt")
    else
        echo "Error: INSTANCE_IP not set and aws_instance_ip.txt not found"
        exit 1
    fi
fi

if [ ! -f "$KEY_FILE" ]; then
    echo "Error: Key file not found: $KEY_FILE"
    exit 1
fi

echo "=== Syncing Changes to AWS Instance ==="
echo "Instance IP: $INSTANCE_IP"
echo "Branch: $BRANCH"
echo ""

# Check current branch locally
CURRENT_BRANCH=$(git -C "$REPO_ROOT" branch --show-current)
if [ "$CURRENT_BRANCH" != "$BRANCH" ]; then
    echo "Warning: Local branch is '$CURRENT_BRANCH', not '$BRANCH'"
    read -p "Continue anyway? (y/n) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

# Add AWS scripts to git (if not already added)
echo "Adding AWS scripts to git..."
cd "$REPO_ROOT"
git add scripts/aws/ 2>/dev/null || true

# Check if there are changes to commit
if git diff --staged --quiet && git diff --quiet; then
    echo "No changes to commit."
else
    echo "Changes detected. Commit and push? (y/n)"
    read -p "" -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        echo "Committing changes..."
        git commit -m "Add AWS deployment scripts and instance management" || {
            echo "Commit failed or no changes to commit"
        }
        
        echo "Pushing to origin/$BRANCH..."
        git push origin "$BRANCH" || {
            echo "Push failed. Continue anyway? (y/n)"
            read -p "" -n 1 -r
            echo
            if [[ ! $REPLY =~ ^[Yy]$ ]]; then
                exit 1
            fi
        }
    fi
fi

# Pull on AWS instance
echo ""
echo "Pulling latest changes on AWS instance..."
ssh -i "$KEY_FILE" ec2-user@"$INSTANCE_IP" << EOF
cd /home/ec2-user/osprey-fork_modern

# Check current branch
CURRENT_BRANCH=\$(git branch --show-current)
echo "Current branch on instance: \$CURRENT_BRANCH"

# Switch to develop if needed
if [ "\$CURRENT_BRANCH" != "$BRANCH" ]; then
    echo "Switching to $BRANCH branch..."
    git checkout $BRANCH || git checkout -b $BRANCH origin/$BRANCH
fi

# Pull latest changes
echo "Pulling latest changes..."
git pull origin $BRANCH

echo "Repository synced successfully"
EOF

echo ""
echo "=== Sync Complete ==="

