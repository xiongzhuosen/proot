#!/bin/bash
# generate-perm-config.sh
# Generate a permission configuration file from a rootfs
#
# Usage:
#   ./generate-perm-config.sh /path/to/rootfs > permissions.conf
#
# This script scans the rootfs and generates a permission configuration
# that can be used with proot's --perm-config option.
#
# Options:
#   ROOTFS          Path to the root filesystem (required)
#   PATTERNS        File containing path patterns to include (optional)
#                   If not specified, scans common directories
#
# Example:
#   ./generate-perm-config.sh /data/data/com.termux/files/home/rootfs > my-perms.conf
#   proot -0 --perm-config my-perms.conf -R /data/data/com.termux/files/home/rootfs /bin/bash

set -e

ROOTFS="${1:-}"
if [ -z "$ROOTFS" ]; then
    echo "Usage: $0 /path/to/rootfs" >&2
    echo "" >&2
    echo "Scans rootfs and generates a proot permission configuration file." >&2
    echo "The output can be used with proot's --perm-config option." >&2
    exit 1
fi

# Remove trailing slash
ROOTFS="${ROOTFS%/}"

echo "# proot Permission Configuration"
echo "# Generated from: $ROOTFS"
echo "# Date: $(date)"
echo ""
echo "@rootfs $ROOTFS"
echo ""

# Directories to scan (common locations for service data)
SCAN_DIRS=(
    "/var/lib"
    "/var/log"
    "/etc"
    "/run"
    "/srv"
    "/opt"
    "/tmp"
)

# Scan each directory
for dir in "${SCAN_DIRS[@]}"; do
    full_path="${ROOTFS}${dir}"
    if [ -d "$full_path" ]; then
        # Find all files and directories, output in ls -l style
        find "$full_path" -maxdepth 2 -print0 2>/dev/null | while IFS= read -r -d '' file; do
            # Get file info
            stat_output=$(stat -c '%A %U %G %n' "$file" 2>/dev/null) || continue
            
            # Parse stat output
            mode=$(echo "$stat_output" | awk '{print $1}')
            user=$(echo "$stat_output" | awk '{print $2}')
            group=$(echo "$stat_output" | awk '{print $3}')
            path=$(echo "$stat_output" | awk '{print $4}')
            
            # Convert to rootfs-relative path
            rel_path="${path#${ROOTFS}}"
            
            # Output in ls -l style format
            echo "${mode}  ${user}  ${group}  ${rel_path}"
        done
    fi
done

# Also scan for specific service directories if they exist
SERVICES=(
    "postgresql"
    "mysql"
    "mariadb"
    "nginx"
    "apache2"
    "redis"
    "mongodb"
)

for service in "${SERVICES[@]}"; do
    # Check common locations
    for base in "/var/lib" "/var/log" "/etc" "/run"; do
        full_path="${ROOTFS}${base}/${service}"
        if [ -d "$full_path" ] || [ -f "$full_path" ]; then
            echo "# ${service} service files found in ${base}/${service}"
            find "${full_path}" -print0 2>/dev/null | while IFS= read -r -d '' file; do
                stat_output=$(stat -c '%A %U %G %n' "$file" 2>/dev/null) || continue
                mode=$(echo "$stat_output" | awk '{print $1}')
                user=$(echo "$stat_output" | awk '{print $2}')
                group=$(echo "$stat_output" | awk '{print $3}')
                path=$(echo "$stat_output" | awk '{print $4}')
                rel_path="${path#${ROOTFS}}"
                echo "${mode}  ${user}  ${group}  ${rel_path}"
            done
        fi
    done
done
