# PRoot - 带权限配置支持的构建产物

## 文件说明

| 文件 | 说明 |
|------|------|
| `proot-x86_64` | x86_64 原生构建版本（用于测试） |
| `example-permissions.conf` | 权限配置文件示例 |
| `../proot` | 最终编译好的 aarch64 版本（需要在 Termux 或交叉编译后生成） |

## 新增功能：`--perm-config`

### 问题背景

原生 PRoot 使用 `-0`（伪造 root）时，所有属于当前用户的文件都会映射为 root 所有。这导致在更新 PostgreSQL 等需要特定用户所有权的软件时出现问题。

### 解决方案

新增 `--perm-config` 参数，允许指定权限配置文件，为特定路径配置自定义的 UID/GID。

### 使用方法

```bash
proot -0 --perm-config /path/to/perms.conf -R /path/to/rootfs /bin/bash
```

### 配置文件格式

```
# 注释行以 # 开头
# 格式：path_pattern uid gid
# 路径模式支持通配符：* ? [...]
# uid/gid 为数字，使用 '-' 保持默认行为

# PostgreSQL 示例
/var/lib/postgresql      70 70
/var/lib/postgresql/*    70 70

# MySQL 示例
/var/lib/mysql           999 999
```

### 优先级

1. **Meta 文件**：如果文件在 proot 内通过 chown 修改过，使用 meta 文件中的权限
2. **权限配置**：如果路径匹配配置文件中的规则，使用配置的 UID/GID
3. **默认映射**：如果文件属于当前用户，映射为伪造的 root

## 交叉编译

### 在 Termux 中直接编译

```bash
pkg install make gcc libtalloc-dev
./build-termux.sh
```

### 使用 Android NDK 交叉编译

```bash
NDK_PATH=/path/to/android-ndk ./build-aarch64.sh
```

## 测试

```bash
# 验证二进制文件
./proot --help

# 验证 --perm-config 选项
./proot --help | grep perm-config

# 创建测试配置
cat > /tmp/test-perms.conf << 'EOF'
/tmp/testfile 1000 1000
EOF

# 测试
./proot -0 --perm-config /tmp/test-perms.conf -R / ls -la /tmp/
```
