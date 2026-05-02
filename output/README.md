# PRoot with Permission Config Support

This is a modified version of PRoot with a new `--perm-config` option that allows
configuring file ownership and permissions for specific paths inside the proot
environment.

## Problem Solved

When running proot as a non-root user with `-0` (fake root), all files owned by
the host user appear as root-owned inside proot. This causes issues with services
like PostgreSQL that refuse to run with root-owned data directories.

## Solution

The `--perm-config` option loads a configuration file that specifies custom
uid/gid/mode for specific paths. When a rule matches a path, the default uid
mapping is skipped, allowing files to retain their real host ownership or use
configured values.

## Usage

```bash
# Basic usage with fake root and permission config
proot -0 -r /path/to/rootfs --perm-config /path/to/perm-config.conf /bin/bash

# Example for PostgreSQL
proot -0 -r /path/to/rootfs --perm-config ./postgres-perms.conf /bin/bash
```

## Configuration File Format

```
# Comments start with #

# Exact path match with full ownership
path=/var/lib/postgresql uid=999 gid=999 mode=0700

# Directory and all children (recursive)
/var/lib/postgresql/** uid=999 gid=999

# Only set specific attributes
/etc/shadow mode=0640
```

## Files

- `proot` - Static aarch64 binary (1MB, no dependencies)
- `loader-aarch64` - PRoot loader (needed if using PROOT_UNBUNDLE_LOADER)
- `perm-config-example.conf` - Example configuration file with documentation

## Build from Source

```bash
cd src
make clean
CROSS_COMPILE=aarch64-linux-gnu- \
CC="aarch64-linux-gnu-gcc" \
CFLAGS="-O2 -static" \
CPPFLAGS="-D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE -I. -I./ -I/path/to/aarch64-talloc-include" \
LDFLAGS="-L/path/to/aarch64-talloc-lib -ltalloc -static" \
PROOT_UNBUNDLE_LOADER=/tmp/proot-loader \
HAS_LOADER_32BIT= \
make
```

## Testing on Termux

1. Copy `proot` to your device:
   ```bash
   adb push output/proot /data/data/com.termux/files/home/
   chmod +x ~/proot
   ```

2. Create a permission config file:
   ```bash
   cat > ~/postgres-perms.conf << 'EOF'
   /var/lib/postgresql/** uid=999 gid=999
   EOF
   ```

3. Run proot with the config:
   ```bash
   ~/proot -0 -r /path/to/rootfs --perm-config ~/postgres-perms.conf /bin/bash
   ```
