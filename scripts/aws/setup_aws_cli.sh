#!/bin/bash
# Setup AWS CLI with credentials

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

echo "=== AWS CLI Setup ==="

# Check if AWS CLI is installed
if ! command -v aws &> /dev/null; then
    echo "AWS CLI not found. Installing..."
    
    # Detect OS
    if [[ "$OSTYPE" == "linux-gnu"* ]]; then
        # Linux (including WSL)
        if command -v apt-get &> /dev/null; then
            sudo apt-get update
            sudo apt-get install -y awscli
        elif command -v yum &> /dev/null; then
            sudo yum install -y awscli
        else
            echo "Error: Package manager not found. Please install AWS CLI manually."
            echo "See: https://aws.amazon.com/cli/"
            exit 1
        fi
    else
        echo "Error: Unsupported OS. Please install AWS CLI manually."
        echo "See: https://aws.amazon.com/cli/"
        exit 1
    fi
fi

echo "AWS CLI version:"
aws --version

# Check for credentials file
AWS_CREDS_FILE="$HOME/.aws/credentials"
AWS_CONFIG_FILE="$HOME/.aws/config"

mkdir -p "$HOME/.aws"

# Check if credentials file exists with the profile
if [ -f "$AWS_CREDS_FILE" ] && grep -q "205930609292_AdministratorAccess" "$AWS_CREDS_FILE"; then
    echo "AWS credentials already configured."
else
    echo "Setting up AWS credentials..."
    
    # Read credentials from aws_keys.md if it exists
    KEYS_FILE="$REPO_ROOT/aws_keys.md"
    if [ ! -f "$KEYS_FILE" ]; then
        echo "Error: aws_keys.md not found at $KEYS_FILE"
        echo "Please create this file with your AWS credentials."
        exit 1
    fi
    
    # Extract credentials (basic extraction - user should verify)
    echo ""
    echo "Please manually add your credentials to ~/.aws/credentials"
    echo "Or run: aws configure --profile 205930609292_AdministratorAccess"
    echo ""
    echo "From aws_keys.md, use:"
    echo "  aws_access_key_id=ASIAS74TLXKGCEDRFVAW"
    echo "  aws_secret_access_key=orfFTmNzZA/bjj+M2XhjjMfuEYKnVbMDupYEOXqn"
    echo "  aws_session_token=<from aws_keys.md>"
    echo ""
    read -p "Press Enter after you've configured credentials..."
fi

# Set default region
if [ ! -f "$AWS_CONFIG_FILE" ]; then
    cat > "$AWS_CONFIG_FILE" << EOF
[default]
region = us-east-1
output = json

[profile 205930609292_AdministratorAccess]
region = us-east-1
output = json
EOF
    echo "Created AWS config file at $AWS_CONFIG_FILE"
fi

# Test AWS connection
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
echo "=== AWS CLI Setup Complete ==="
echo "Default profile: 205930609292_AdministratorAccess"
echo "Default region: us-east-1"

