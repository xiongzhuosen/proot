# Proot ARM64 交叉编译完整指南

> **目标**: 任何开发者（无项目记忆）可按此文档从零编译出完全相同的静态链接 aarch64 proot 二进制文件。

---

## 1. 环境准备

### 宿主机要求
- **OS**: Linux x86_64 (Ubuntu/Debian 推荐)
- **磁盘**: 至少 4GB 可用空间

### 安装基础依赖

```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install -y \
  git make gcc g++ wget curl pkg-config \
  build-essential autoconf automake \
  python3 python3-pip \
  qemu-user-static
```

> **注意**: `qemu-user-static` 是 talloc 交叉编译测试运行所必需的。

---

## 2. 交叉编译工具链

### 2.1 下载并解压

```bash
cd /tmp
wget https://more.musl.cc/10/x86_64-linux-musl/aarch64-linux-musl-cross.tgz
tar -xzf aarch64-linux-musl-cross.tgz
```

### 2.2 验证工具链

```bash
/tmp/aarch64-linux-musl-cross/bin/aarch64-linux-musl-gcc --version
```

**预期输出**: `aarch64-linux-musl-gcc (GCC) 10.x.x`

**工具链路径**: `/tmp/aarch64-linux-musl-cross/`
- 编译器: `bin/aarch64-linux-musl-gcc`
- 链接器: `bin/aarch64-linux-musl-ld`
- sysroot: `aarch64-linux-musl/`

---

## 3. 编译 talloc (musl 版本)

talloc 是 proot 的唯一外部依赖。由于 proot 使用自定义 fake_id0 扩展，必须编译静态库。

### 3.1 下载 talloc 源码

```bash
cd /tmp
git clone https://git.samba.org/talloc.git
cd talloc
git checkout talloc-2.4.1
```

### 3.2 配置交叉编译环境

创建交叉编译答案文件 `/tmp/cross-answers.txt`:

```bash
cat > /tmp/cross-answers.txt << 'EOF'
Checking uname machine type: "aarch64"
Checking uname system type: "linux"
Checking getconf LFS_CFLAGS: ""
Checking for large file support: yes
Checking for fseeko: yes
Checking for strerror: yes
Checking for va_copy: yes
Checking for fdatasync [up to 1 arguments]: yes
Checking for memmove: yes
Checking for pread [up to 3 arguments]: yes
Checking for readdir [up to 1 arguments]: yes
Checking for strndup: yes
Checking for strnlen: yes
Checking for working strndup: yes
Checking for library rt: no
Checking for library iconv: no
Checking for -lm: yes
Checking for library dl: no
Checking for library util: no
Checking for -lpthread: no
Checking for internal libcc: no
Checking for shared library: no
EOF
```

### 3.3 编译 talloc

```bash
cd /tmp/talloc

export CC=/tmp/aarch64-linux-musl-cross/bin/aarch64-linux-musl-gcc
export AR=/tmp/aarch64-linux-musl-cross/bin/aarch64-linux-musl-ar
export RANLIB=/tmp/aarch64-linux-musl-cross/bin/aarch64-linux-musl-ranlib

./configure \
  --cross-execute="qemu-aarch64 -L /tmp/aarch64-linux-musl-cross/aarch64-linux-musl" \
  --cross-answers=/tmp/cross-answers.txt \
  --without-gettext \
  --enable-static \
  --disable-shared

make
```

### 3.4 验证产物

```bash
file bin/default/libtalloc.a
# 预期输出: current ar archive

ls bin/default/libtalloc.a
# 确认文件存在
```

**编译产物路径**: `/tmp/talloc/bin/default/libtalloc.a`
**头文件路径**: `/tmp/talloc/include/talloc.h`

---

## 4. 获取 proot 源码

```bash
git clone https://github.com/proot-me/proot.git
cd proot
git checkout master
```

> **注意**: 本项目的构建系统在 `src/` 目录下，根目录仅包含文档和配置。

---

## 5. 源码修改

以下修改必须全部应用。每处修改都提供完整的文件内容或精确的 diff。

### 5.1 修复 Android 临时目录问题

**文件**: `src/path/temp.c`

**修改内容**: 在 `get_temp_directory()` 函数中，`#ifdef __ANDROID__` 分支必须包含以下代码：

```c
#ifdef __ANDROID__
	cand = getenv("PREFIX");
	if (cand && cand[0] != '\0') {
		char *prefix_tmp = talloc_asprintf(talloc_autofree_context(), "%s/tmp", cand);
		if (prefix_tmp) {
			temp_directory = prefix_tmp;
			return temp_directory;
		}
	}
	temp_directory = "/data/data/com.termux/files/usr/tmp";
	return temp_directory;
#endif
```

**精确位置**: 在 `cand = getenv("PROOT_TMP_DIR");` 检查块之后（约第 29 行），`cand = getenv("TMPDIR");` 之前（约第 44 行）。

**完整文件**: `src/path/temp.c` 共 375 行。该修改仅影响 `get_temp_directory()` 函数的前 52 行。

### 5.2 fake_id0 权限配置功能

以下文件必须存在于项目中：

#### 5.2.1 `src/extension/fake_id0/perm_config.h`

```c
#ifndef PERM_CONFIG_H
#define PERM_CONFIG_H

#include <sys/types.h>
#include <stdbool.h>

#define MAX_PERM_RULES 256
#define MAX_PATH_LEN 4096

typedef enum {
	MATCH_EXACT,    /* exact path match */
	MATCH_PREFIX,   /* directory and all children (path/**) */
} MatchType;

typedef struct {
	char path[MAX_PATH_LEN];
	MatchType match_type;
	uid_t uid;
	gid_t gid;
	mode_t mode;
	bool has_uid;
	bool has_gid;
	bool has_mode;
} PermRule;

typedef struct {
	PermRule rules[MAX_PERM_RULES];
	int rule_count;
	char config_path[MAX_PATH_LEN];
} PermConfig;

extern int load_perm_config(PermConfig *config, const char *path);
extern bool find_perm_rule(const PermConfig *config, const char *filepath,
			   PermRule *out_rule);

#endif /* PERM_CONFIG_H */
```

#### 5.2.2 `src/extension/fake_id0/perm_config.c`

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#include "extension/fake_id0/perm_config.h"

/* skip leading whitespace */
static const char *skip_ws(const char *s)
{
	while (*s && isspace((unsigned char)*s))
		s++;
	return s;
}

/* parse a line like:
 *   path=/var/lib/postgresql uid=999 gid=999 mode=0700
 *   /var/lib/postgresql/** uid=999 gid=999
 *   /var/lib/postgresql/data uid=999
 *
 * supports:
 *   path=VALUE   - exact match (default)
 *   VALUE        - path, if ends with /** it's prefix match
 *   uid=NUMBER
 *   gid=NUMBER
 *   mode=OCTAL
 */
static int parse_perm_line(const char *line, PermRule *rule)
{
	const char *p = line;
	const char *end;
	char buf[MAX_PATH_LEN];

	memset(rule, 0, sizeof(*rule));

	while (1) {
		p = skip_ws(p);
		if (*p == '\0' || *p == '#')
			break;

		/* uid=NUMBER */
		if (strncmp(p, "uid=", 4) == 0) {
			p += 4;
			rule->uid = (uid_t)strtoul(p, (char **)&end, 10);
			if (end == p)
				return -1;
			p = end;
			rule->has_uid = true;
			continue;
		}

		/* gid=NUMBER */
		if (strncmp(p, "gid=", 4) == 0) {
			p += 4;
			rule->gid = (gid_t)strtoul(p, (char **)&end, 10);
			if (end == p)
				return -1;
			p = end;
			rule->has_gid = true;
			continue;
		}

		/* mode=OCTAL */
		if (strncmp(p, "mode=", 5) == 0) {
			p += 5;
			rule->mode = (mode_t)strtoul(p, (char **)&end, 8);
			if (end == p)
				return -1;
			p = end;
			rule->has_mode = true;
			continue;
		}

		/* path=VALUE */
		if (strncmp(p, "path=", 5) == 0) {
			p += 5;
			end = p;
			while (*end && !isspace((unsigned char)*end))
				end++;
			size_t len = end - p;
			if (len >= MAX_PATH_LEN)
				return -1;
			memcpy(buf, p, len);
			buf[len] = '\0';
			p = end;
			goto set_path;
		}

		/* bare path (until whitespace) */
		end = p;
		while (*end && !isspace((unsigned char)*end))
			end++;
		size_t len = end - p;
		if (len >= MAX_PATH_LEN)
			return -1;
		memcpy(buf, p, len);
		buf[len] = '\0';
		p = end;

	set_path:
		/* check for /** suffix => prefix match */
		size_t blen = strlen(buf);
		if (blen >= 3 && strcmp(buf + blen - 3, "/**") == 0) {
			buf[blen - 3] = '\0';
			rule->match_type = MATCH_PREFIX;
		} else {
			rule->match_type = MATCH_EXACT;
		}
		strncpy(rule->path, buf, MAX_PATH_LEN - 1);
		rule->path[MAX_PATH_LEN - 1] = '\0';
	}

	if (rule->path[0] == '\0')
		return -1; /* no path specified */

	return 0;
}

int load_perm_config(PermConfig *config, const char *path)
{
	FILE *fp;
	char line[4096];
	int line_num = 0;

	memset(config, 0, sizeof(*config));
	strncpy(config->config_path, path, MAX_PATH_LEN - 1);
	config->config_path[MAX_PATH_LEN - 1] = '\0';

	fp = fopen(path, "r");
	if (!fp) {
		return -errno;
	}

	while (fgets(line, sizeof(line), fp)) {
		line_num++;

		/* strip trailing newline */
		size_t len = strlen(line);
		if (len > 0 && line[len - 1] == '\n')
			line[len - 1] = '\0';

		/* skip empty lines and comments */
		const char *p = skip_ws(line);
		if (*p == '\0' || *p == '#')
			continue;

		if (config->rule_count >= MAX_PERM_RULES) {
			fclose(fp);
			return -ENOMEM;
		}

		if (parse_perm_line(line, &config->rules[config->rule_count]) < 0) {
			/* skip malformed lines silently */
			continue;
		}

		config->rule_count++;
	}

	fclose(fp);
	return config->rule_count;
}

/* Find the most specific matching rule for the given filepath.
 * Returns true if a rule matches, false otherwise.
 * Exact matches take priority over prefix matches.
 */
bool find_perm_rule(const PermConfig *config, const char *filepath,
		    PermRule *out_rule)
{
	int i;
	const PermRule *best_exact = NULL;
	const PermRule *best_prefix = NULL;
	size_t best_prefix_len = 0;

	if (!config || !filepath || config->rule_count <= 0)
		return false;

	size_t flen = strlen(filepath);

	for (i = 0; i < config->rule_count; i++) {
		const PermRule *rule = &config->rules[i];

		if (rule->match_type == MATCH_EXACT) {
			if (strcmp(rule->path, filepath) == 0) {
				best_exact = rule;
				/* exact match found, can't get better */
				goto found;
			}
		} else {
			/* MATCH_PREFIX: check if filepath starts with rule->path/ */
			size_t plen = strlen(rule->path);
			if (plen < flen &&
			    strncmp(filepath, rule->path, plen) == 0 &&
			    filepath[plen] == '/') {
				if (plen > best_prefix_len) {
					best_prefix = rule;
					best_prefix_len = plen;
				}
			}
		}
	}

	if (best_exact) {
found:
		if (out_rule)
			*out_rule = *best_exact;
		return true;
	}

	if (best_prefix) {
		if (out_rule)
			*out_rule = *best_prefix;
		return true;
	}

	return false;
}
```

#### 5.2.3 修改 `src/extension/fake_id0/config.h`

确保文件内容如下：

```c
#ifndef FAKE_ID0_CONFIG_H
#define FAKE_ID0_CONFIG_H

#include <sys/types.h>   /* uid_t, gid_t */
#include "extension/fake_id0/perm_config.h"

typedef struct {
	uid_t ruid;
	uid_t euid;
	uid_t suid;
	uid_t fsuid;

	gid_t rgid;
	gid_t egid;
	gid_t sgid;
	gid_t fsgid;

	mode_t umask;

	/* permission configuration for specific paths */
	PermConfig perm_config;
	bool has_perm_config;
} Config;

#endif /* FAKE_ID0_CONFIG_H */
```

#### 5.2.4 修改 `src/cli/proot.c`

添加 `--perm-config` 参数处理函数。在 `handle_option_port_switch` 函数之后（约第 339 行）添加：

```c
static int handle_option_perm_config(Tracee *tracee, const Cli *cli UNUSED, const char *value)
{
	void *extension;

	extension = get_extension(tracee, fake_id0_callback);
	if (extension == NULL) {
		/* fake_id0 not enabled yet, enable it with default uid:gid */
		(void) initialize_extension(tracee, fake_id0_callback, "0:0");
		extension = get_extension(tracee, fake_id0_callback);
		if (extension == NULL)
			return -1;
	}

	/* Store the perm_config path in the extension's config */
	Extension *ext = (Extension *)extension;
	Config *config = talloc_get_type_abort(ext->config, Config);

	int status = load_perm_config(&config->perm_config, value);
	if (status < 0) {
		note(tracee, ERROR, USER, "failed to load perm config: %s", value);
		return -1;
	}
	config->has_perm_config = true;

	note(tracee, INFO, USER, "perm config loaded: %s (%d rules)", value, status);
	return 0;
}
```

同时在 CLI 选项注册部分（`cli_options` 数组）添加：

```c
{ "perm-config", 'P', "<path>", handle_option_perm_config },
```

#### 5.2.5 修改 `src/GNUmakefile`

确保 `OBJECTS` 列表中包含以下行：

```makefile
	extension/fake_id0/perm_config.o \
```

该行应在 `extension/fake_id0/fake_id0.o` 之前。

---

## 6. 编译 proot

### 6.1 设置环境变量

```bash
cd /path/to/proot  # 项目根目录

export CC=/tmp/aarch64-linux-musl-cross/bin/aarch64-linux-musl-gcc
export AR=/tmp/aarch64-linux-musl-cross/bin/aarch64-linux-musl-ar
export RANLIB=/tmp/aarch64-linux-musl-cross/bin/aarch64-linux-musl-ranlib
export STRIP=/tmp/aarch64-linux-musl-cross/bin/aarch64-linux-musl-strip
```

### 6.2 编译

```bash
# 清理旧构建产物
make -C src clean

# 编译
make -C src \
  CC="$CC" \
  AR="$AR" \
  RANLIB="$RANLIB" \
  STRIP="$STRIP" \
  CFLAGS="-I/tmp/talloc/include -static" \
  LDFLAGS="-L/tmp/talloc/bin/default -ltalloc -static"
```

> **重要**: 构建命令必须在项目根目录执行，使用 `make -C src`。`src/` 目录包含 `GNUmakefile`。

### 6.3 验证产物

```bash
# 检查架构和链接类型
file src/proot
# 预期输出: ELF 64-bit LSB executable, ARM aarch64, statically linked, ...

# 检查符号表 (可选)
readelf -h src/proot | grep -E "Class|Machine|Type"
# 预期:
#   Class:                             ELF64
#   Machine:                           AArch64
#   Type:                              EXEC (Executable file)
```

### 6.4 复制到输出目录

```bash
mkdir -p output
cp src/proot output/proot
```

---

## 7. 测试 (可选)

### 7.1 使用 QEMU 测试

```bash
# 在宿主机上使用 QEMU 运行 proot
qemu-aarch64 -L /tmp/aarch64-linux-musl-cross/aarch64-linux-musl output/proot --help
```

### 7.2 部署到 Termux 设备

```bash
# 方法 1: 使用 adb
adb push output/proot /data/data/com.termux/files/usr/bin/proot
adb shell chmod +x /data/data/com.termux/files/usr/bin/proot

# 方法 2: 手动复制
# 将 output/proot 复制到设备存储，然后在 Termux 中执行:
cp /sdcard/proot $PREFIX/bin/
chmod +x $PREFIX/bin/proot
```

### 7.3 验证 Termux 中运行

```bash
# 在 Termux 中执行
proot --help
# 应显示帮助信息

# 测试 chroot
proot -r /path/to/rootfs /bin/echo "Hello from proot"
```

---

## 8. 已知问题及解决方案

### Q1: talloc 配置失败

**错误**: `configure: error: cannot run test program while cross compiling`

**原因**: `cross-answers.txt` 缺少必要的配置项。

**解决**: 确保 `/tmp/cross-answers.txt` 包含所有必需的测试答案。参考步骤 3.2。

### Q2: 编译时找不到 talloc.h

**错误**: `fatal error: talloc.h: No such file or directory`

**解决**: 验证头文件路径：
```bash
ls /tmp/talloc/include/talloc.h
```

确保 `CFLAGS` 中包含 `-I/tmp/talloc/include`。

### Q3: 链接时找不到 talloc 符号

**错误**: `undefined reference to 'talloc_*'`

**解决**: 验证静态库路径：
```bash
ls /tmp/talloc/bin/default/libtalloc.a
```

确保 `LDFLAGS` 中包含 `-L/tmp/talloc/bin/default -ltalloc`。

### Q4: 启动时崩溃 (SIGSYS)

**原因**: 使用了 glibc 而非 musl。

**解决**: 确保工具链是 `aarch64-linux-musl-gcc`，不是 `aarch64-linux-gnu-gcc`。验证方法：
```bash
$CC --version
# 输出应包含 "musl"
```

### Q5: 启动延迟 20 秒+

**原因**: Kali chroot 环境中缺少 `en_US.UTF-8` locale 文件，glibc 会反复尝试加载（300+ 次 ENOENT 错误）。即使设置了 `LANG=en_US.UTF-8`，如果没有 `LC_ALL`，glibc 仍会查找 locale 文件。

**解决**: proot 已内置自动修复：在 execve 阶段自动注入 `LC_ALL=C` 环境变量（仅当 LC_ALL 未设置时），跳过 glibc locale 初始化。无需手动生成 locale。

### Q6: "can't create temporary directory" 错误

**原因**: Termux 环境中 `/tmp` 目录不存在。

**解决**: 确保已应用 5.1 节的 `src/path/temp.c` 修改。该修改会强制使用 `$PREFIX/tmp` 或 `/data/data/com.termux/files/usr/tmp`。

---

## 9. 构建系统说明

### 目录结构

```
proot/
├── src/
│   ├── GNUmakefile      # 主构建文件
│   ├── cli/             # 命令行处理
│   ├── execve/          # execve 模拟
│   ├── path/            # 路径转换
│   ├── syscall/         # 系统调用处理
│   ├── tracee/          # 被跟踪进程管理
│   ├── ptrace/          # ptrace 封装
│   └── extension/       # 扩展模块
│       └── fake_id0/    # fake root 扩展
├── output/              # 编译产物目录
├── BUILD.md             # 本文档
└── DEVELOPMENT_HISTORY.md  # 开发历史
```

### 关键构建参数

| 参数 | 说明 | 默认值 |
|------|------|--------|
| `CC` | C 编译器 | `gcc` |
| `AR` | 归档工具 | `ar` |
| `RANLIB` | 索引生成 | `ranlib` |
| `STRIP` | 符号剥离 | `strip` |
| `CFLAGS` | 编译器标志 | `-Wall -Wextra -O2` |
| `LDFLAGS` | 链接器标志 | `-ltalloc -Wl,-z,noexecstack` |
| `CPPFLAGS` | 预处理器标志 | `-D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE` |

### 构建产物

| 文件 | 说明 |
|------|------|
| `src/proot` | 主可执行文件 |
| `src/loader/loader` | aarch64 loader (自动嵌入到 proot) |
| `src/build.h` | 自动生成的版本信息头文件 |

---

## 10. 完整命令汇总

为方便复制，以下是所有关键命令的汇总：

```bash
# 1. 安装依赖
sudo apt-get update
sudo apt-get install -y git make gcc g++ wget curl pkg-config \
  build-essential autoconf automake python3 python3-pip qemu-user-static

# 2. 下载工具链
cd /tmp
wget https://more.musl.cc/10/x86_64-linux-musl/aarch64-linux-musl-cross.tgz
tar -xzf aarch64-linux-musl-cross.tgz

# 3. 下载并编译 talloc
git clone https://git.samba.org/talloc.git
cd talloc
git checkout talloc-2.4.1

cat > /tmp/cross-answers.txt << 'EOF'
Checking uname machine type: "aarch64"
Checking uname system type: "linux"
Checking getconf LFS_CFLAGS: ""
Checking for large file support: yes
Checking for fseeko: yes
Checking for strerror: yes
Checking for va_copy: yes
Checking for fdatasync [up to 1 arguments]: yes
Checking for memmove: yes
Checking for pread [up to 3 arguments]: yes
Checking for readdir [up to 1 arguments]: yes
Checking for strndup: yes
Checking for strnlen: yes
Checking for working strndup: yes
Checking for library rt: no
Checking for library iconv: no
Checking for -lm: yes
Checking for library dl: no
Checking for library util: no
Checking for -lpthread: no
Checking for internal libcc: no
Checking for shared library: no
EOF

export CC=/tmp/aarch64-linux-musl-cross/bin/aarch64-linux-musl-gcc
export AR=/tmp/aarch64-linux-musl-cross/bin/aarch64-linux-musl-ar
export RANLIB=/tmp/aarch64-linux-musl-cross/bin/aarch64-linux-musl-ranlib

./configure \
  --cross-execute="qemu-aarch64 -L /tmp/aarch64-linux-musl-cross/aarch64-linux-musl" \
  --cross-answers=/tmp/cross-answers.txt \
  --without-gettext \
  --enable-static \
  --disable-shared

make

# 4. 获取 proot 源码
cd /path/to/workspace
git clone https://github.com/proot-me/proot.git
cd proot
git checkout master

# 5. 应用源码修改 (见第 5 节)

# 6. 编译 proot
export CC=/tmp/aarch64-linux-musl-cross/bin/aarch64-linux-musl-gcc
export AR=/tmp/aarch64-linux-musl-cross/bin/aarch64-linux-musl-ar
export RANLIB=/tmp/aarch64-linux-musl-cross/bin/aarch64-linux-musl-ranlib
export STRIP=/tmp/aarch64-linux-musl-cross/bin/aarch64-linux-musl-strip

make -C src clean
make -C src \
  CC="$CC" \
  AR="$AR" \
  RANLIB="$RANLIB" \
  STRIP="$STRIP" \
  CFLAGS="-I/tmp/talloc/include -static" \
  LDFLAGS="-L/tmp/talloc/bin/default -ltalloc -static"

# 7. 验证和复制
file src/proot
mkdir -p output
cp src/proot output/proot
```
