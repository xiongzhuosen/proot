# PRoot - User-mode chroot for Termux

A patched version of [PRoot](https://github.com/proot-me/PRowt/) with enhanced permission configuration support, optimized for [Termux](https://termux.com).

## Features

- **Permission Configuration**: Configure file ownership and permissions inside proot using a configuration file
- **Multiple Config Formats**: Support for ls -l style, numeric, and simplified formats
- **Username/Groupname Resolution**: Automatically loads /etc/passwd and /etc/group from rootfs
- **Special Permission Bits**: Full support for setuid, setgid, and sticky bits
- **Pure Static Binary**: No external dependencies, works directly in Termux

## Quick Start

### Basic Usage

```bash
# Run as fake root with permission config
./proot -0 --perm-config permissions.conf -R /path/to/rootfs /bin/bash

# Run with specific UID:GID
./proot -i 1000:1000 --perm-config permissions.conf -R /path/to/rootfs /bin/bash
```

### Permission Configuration

The permission configuration file allows you to specify file ownership and permissions for paths inside proot. This is useful when running services (like PostgreSQL, MySQL, etc.) that require specific file ownership.

#### Configuration Formats

**Format 1 (recommended): path_pattern user:group mode**
```
/var/lib/postgresql  postgres:postgres  0700
/tmp/*               1000:1000          rwxrwxrwt
```

**Format 2 (ls -l style): mode_str user group path**
```
drwx------  postgres  postgres  /var/lib/postgresql
-rw-r--r--  root      root      /etc/passwd
```

**Format 3 (numeric): path_pattern uid:gid mode_octal**
```
/data  70:70  0700
```

**Format 4 (simplified): path_pattern user mode**
```
/var/lib/postgresql  postgres  0755
```

**Format 5 (find -printf style): path user:group:mode**
```
/var/lib/postgresql postgres:postgres:0755
```

#### Mode Formats

- **Octal**: `0755`, `4755` (setuid), `2755` (setgid), `1755` (sticky)
- **Symbolic**: `rwxr-xr-x`, `rwsr-xr-x` (setuid), `rwxr-sr-x` (setgid), `rwxrwxrwt` (sticky)
- **Keep original**: Use `-` to keep the original value

#### User/Group

- **Username**: `postgres`, `root`, `www-data`
- **UID/GID**: `70`, `1000`
- **Keep original**: Use `-` to keep the original value

### Generate Configuration from Rootfs

Use the provided script to generate a permission configuration from your rootfs:

```bash
./generate-perm-config.sh /path/to/rootfs > my-permissions.conf
```

### Example: PostgreSQL Setup

1. Create a permission configuration file:

```bash
cat > pg-perms.conf << 'EOF'
@rootfs /data/data/com.termux/files/home/rootfs

/var/lib/postgresql  postgres:postgres  0700
/var/lib/postgresql/*  postgres:postgres  0700
/var/log/postgresql  postgres:postgres  0755
/etc/postgresql  postgres:postgres  0755
/etc/postgresql/*  postgres:postgres  0644
EOF
```

2. Run proot with the configuration:

```bash
./proot -0 --perm-config pg-perms.conf -R /data/data/com.termux/files/home/rootfs /bin/bash
```

## Building from Source

### Prerequisites

- Android NDK or cross-compilation toolchain (`gcc-aarch64-linux-gnu`)
- libtalloc for aarch64
- gawk (for strtonum support)

### Build

```bash
# Install dependencies (Debian/Ubuntu)
sudo apt-get install -y gcc-aarch64-linux-gnu gawk

# Download aarch64 talloc
cd /tmp
curl -sL "http://deb.debian.org/debian/pool/main/t/talloc/libtalloc-dev_2.4.0-f2_arm64.deb" -o libtalloc-dev_arm64.deb
curl -sL "http://deb.debian.org/debian/pool/main/t/talloc/libtalloc2_2.4.0-f2_arm64.deb" -o libtalloc2_arm64.deb
dpkg-deb -x libtalloc-dev_arm64.deb /tmp/talloc-arm64
dpkg-deb -x libtalloc2_arm64.deb /tmp/talloc2-arm64

# Build
cd src
make clean
make \
    CC=aarch64-linux-gnu-gcc \
    AR=aarch64-linux-gnu-ar \
    RANLIB=aarch64-linux-gnu-ranlib \
    STRIP=aarch64-linux-gnu-strip \
    OBJCOPY=aarch64-linux-gnu-objcopy \
    OBJDUMP=aarch64-linux-gnu-objdump \
    CPPFLAGS="-D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE -I. -I. -I/tmp/talloc-arm64/usr/include" \
    CFLAGS="-Wall -Wextra -O2" \
    LDFLAGS="-static -L/tmp/talloc-arm64/usr/lib/aarch64-linux-gnu -ltalloc -Wl,-z,noexecstack"

# Binary is now at src/proot
```

## License

GPL v2 or later. See [COPYING](COPYING) for details.
