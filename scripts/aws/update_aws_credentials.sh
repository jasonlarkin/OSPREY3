#!/bin/bash
# Update AWS credentials from aws_keys.md

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
KEYS_FILE="$REPO_ROOT/aws_keys.md"
CREDS_FILE="$HOME/.aws/credentials"

if [ ! -f "$KEYS_FILE" ]; then
    echo "Error: aws_keys.md not found at $KEYS_FILE"
    exit 1
fi

echo "=== Updating AWS Credentials ==="

# Extract credentials from aws_keys.md
# Look for the credentials block in Option 2 format
ACCESS_KEY=$(grep -A 1 "aws_access_key_id=" "$KEYS_FILE" | grep "aws_access_key_id=" | cut -d'=' -f2 | tr -d ' ')
SECRET_KEY=$(grep -A 1 "aws_secret_access_key=" "$KEYS_FILE" | grep "aws_secret_access_key=" | cut -d'=' -f2 | tr -d ' ')
SESSION_TOKEN=$(grep -A 1 "aws_session_token=" "$KEYS_FILE" | grep "aws_session_token=" | cut -d'=' -f2- | tr -d ' ')

if [ -z "$ACCESS_KEY" ] || [ -z "$SECRET_KEY" ] || [ -z "$SESSION_TOKEN" ]; then
    echo "Error: Could not extract credentials from aws_keys.md"
    echo "Please ensure the file contains aws_access_key_id, aws_secret_access_key, and aws_session_token"
    exit 1
fi

# Create/update credentials file
mkdir -p "$HOME/.aws"

# Backup existing credentials if they exist
if [ -f "$CREDS_FILE" ]; then
    cp "$CREDS_FILE" "$CREDS_FILE.backup.$(date +%Y%m%d_%H%M%S)"
    echo "Backed up existing credentials"
fi

# Update or add the profile
if grep -q "\[205930609292_AdministratorAccess\]" "$CREDS_FILE" 2>/dev/null; then
    # Update existing profile
    sed -i '/\[205930609292_AdministratorAccess\]/,/^\[/ {
        /aws_access_key_id=/c\aws_access_key_id='"$ACCESS_KEY"'
        /aws_secret_access_key=/c\aws_secret_access_key='"$SECRET_KEY"'
        /aws_session_token=/c\aws_session_token='"$SESSION_TOKEN"'
    }' "$CREDS_FILE"
    echo "Updated existing profile"
else
    # Add new profile
    cat >> "$CREDS_FILE" << EOF

[205930609292_AdministratorAccess]
aws_access_key_id=$ACCESS_KEY
aws_secret_access_key=$SECRET_KEY
aws_session_token=$SESSION_TOKEN
EOF
    echo "Added new profile"
fi

echo "Credentials updated in $CREDS_FILE"

# Test connection
echo ""
echo "Testing AWS connection..."
export AWS_PROFILE=205930609292_AdministratorAccess
if aws sts get-caller-identity &> /dev/null; then
    echo "AWS connection successful!"
    aws sts get-caller-identity
else
    echo "Warning: AWS connection test failed. Please check your credentials."
    exit 1
fi

echo ""
echo "=== Credentials Updated Successfully ==="

