#!/bin/bash
# Manage EC2 instance (stop/start/status)

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
PROFILE="${AWS_PROFILE:-205930609292_AdministratorAccess}"

export AWS_PROFILE="$PROFILE"
export AWS_PAGER=""

# Get instance ID
if [ -f "$REPO_ROOT/aws_instance_id.txt" ]; then
    INSTANCE_ID=$(cat "$REPO_ROOT/aws_instance_id.txt")
else
    echo "Error: aws_instance_id.txt not found at $REPO_ROOT/aws_instance_id.txt"
    echo "Instance not provisioned yet?"
    exit 1
fi

ACTION="${1:-status}"

case "$ACTION" in
    start)
        echo "Starting instance: $INSTANCE_ID"
        aws ec2 start-instances --instance-ids "$INSTANCE_ID"
        echo "Waiting for instance to be running..."
        aws ec2 wait instance-running --instance-ids "$INSTANCE_ID"
        
        # Get new public IP (may change after stop/start)
        PUBLIC_IP=$(aws ec2 describe-instances \
            --instance-ids "$INSTANCE_ID" \
            --query 'Reservations[0].Instances[0].PublicIpAddress' \
            --output text)
        echo "$PUBLIC_IP" > "$REPO_ROOT/aws_instance_ip.txt"
        
        echo "Instance started. Public IP: $PUBLIC_IP"
        echo "SSH: ssh -i ~/.ssh/osprey-dev.pem ec2-user@$PUBLIC_IP"
        ;;
    
    stop)
        echo "Stopping instance: $INSTANCE_ID"
        echo "Note: Instance state (EBS volumes) will be preserved."
        aws ec2 stop-instances --instance-ids "$INSTANCE_ID"
        echo "Waiting for instance to be stopped..."
        aws ec2 wait instance-stopped --instance-ids "$INSTANCE_ID"
        echo "Instance stopped. You only pay for EBS storage (~$0.10/GB/month)."
        ;;
    
    status)
        STATE=$(aws ec2 describe-instances \
            --instance-ids "$INSTANCE_ID" \
            --query 'Reservations[0].Instances[0].State.Name' \
            --output text)
        
        if [ "$STATE" == "running" ]; then
            PUBLIC_IP=$(aws ec2 describe-instances \
                --instance-ids "$INSTANCE_ID" \
                --query 'Reservations[0].Instances[0].PublicIpAddress' \
                --output text)
            echo "Instance: $INSTANCE_ID"
            echo "State: $STATE"
            echo "Public IP: $PUBLIC_IP"
            echo "SSH: ssh -i ~/.ssh/osprey-dev.pem ec2-user@$PUBLIC_IP"
        else
            echo "Instance: $INSTANCE_ID"
            echo "State: $STATE"
            echo "To start: $0 start"
        fi
        ;;
    
    terminate)
        echo "WARNING: This will TERMINATE the instance and DELETE all data!"
        read -p "Type 'yes' to confirm: " confirm
        if [ "$confirm" == "yes" ]; then
            aws ec2 terminate-instances --instance-ids "$INSTANCE_ID"
            echo "Instance termination initiated."
            echo "Note: EBS volumes with DeleteOnTermination=false will persist."
        else
            echo "Termination cancelled."
        fi
        ;;
    
    *)
        echo "Usage: $0 {start|stop|status|terminate}"
        echo ""
        echo "Commands:"
        echo "  start     - Start the instance (preserves state)"
        echo "  stop      - Stop the instance (saves compute costs, preserves state)"
        echo "  status    - Show instance status and connection info"
        echo "  terminate - Permanently delete the instance (WARNING: data loss)"
        exit 1
        ;;
esac

