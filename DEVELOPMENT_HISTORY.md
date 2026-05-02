# 开发尝试记录

本文档记录在 proot ARM64 交叉编译和修复过程中的所有尝试。

## 2026-05-02: 初始问题修复

### 问题: set_robust_list SIGSYS 崩溃
**现象**: proot 在 Android 上启动时立即崩溃，strace 显示 `SIGSYS` 信号
**原因**: Android seccomp 过滤器拦截了 `set_robout_list` 系统调用
**尝试方案**:
1. ~~使用 glibc 工具链编译~~ - 失败，glibc 启动时会调用被拦截的系统调用
2. **使用 musl libc 工具链编译** - 成功，musl 更加精简，避免了被拦截的调用

### 工具链切换
- **原工具链**: aarch64-linux-gnu (glibc)
- **新工具链**: aarch64-linux-musl-cross
- **工具链路径**: `/tmp/aarch64-linux-musl-cross/`

## 2026-05-02: talloc 编译问题

### 问题: talloc 与 musl 不兼容
**现象**: 编译时报 `replace.h` 类型冲突
**原因**: talloc 的 `replace.h` 重新定义了 `__int64` 等类型，与 musl 冲突
**尝试方案**:
1. ~~使用系统 talloc~~ - 失败，系统版本是 glibc 编译的
2. **手动编译 musl 版本 talloc** - 成功
   - 下载 samba 源码 (包含 talloc)
   - 使用 musl 交叉编译工具链
   - 生成静态库 `/tmp/musl-talloc-aarch64/libtalloc.a`

## 2026-05-02: statx 系统调用问题

### 问题: statx 头文件缺失
**现象**: 编译时报 `struct statx` 未定义
**原因**: musl libc 旧版本不包含 `statx.h`
**解决方案**: 手动创建 `src/syscall/statx.h` 头文件

## 2026-05-02: Android /tmp 目录问题

### 问题: "can't create temporary directory" 错误
**现象**: proot 启动时报无法创建临时目录
**原因**: Termux 环境中 `/tmp` 不存在
**尝试方案**:
1. ~~动态探测多个临时路径~~ - 失败，逻辑复杂且不可靠
2. **强制使用 `$PREFIX/tmp`** - 成功
   - 修改 `src/path/temp.c`
   - 添加 `__ANDROID__` 分支
   - 优先使用 `$PREFIX/tmp`，回退到 `/data/data/com.termux/files/usr/tmp`

## 2026-05-02: fake_id0 --perm-config 功能

### 新增功能: 权限配置文件支持
**需求**: 支持通过配置文件指定特定路径的权限映射
**实现**:
1. 创建 `src/extension/fake_id0/perm_config.h` - 数据结构定义
2. 创建 `src/extension/fake_id0/perm_config.c` - 配置文件解析
3. 修改 `src/cli/proot.c` - 添加 `--perm-config` CLI 参数
4. 修改 `src/extension/fake_id0/config.h` - 添加 `perm_config` 字段到 Config 结构

**配置文件格式**:
```
# 精确路径匹配
path=/var/lib/postgresql uid=999 gid=999 mode=0700

# 前缀匹配 (递归)
/var/lib/postgresql/** uid=999 gid=999
```

## 2026-05-02: 启动延迟问题 (进行中)

### 问题: 修改后的二进制文件启动延迟 5 秒
**现象**: 用户报告新二进制文件需要 5 秒才能启动，原版本"瞬间进入"
**分析**:
1. strace 日志显示大量 `ENOENT` 错误查找 `en_US.UTF-8` locale (30+ 次)
2. 子进程最终被 `SIGINT` 终止
3. `set_robust_list` 拦截正常工作，无 `SIGSYS` 崩溃

**可能原因**:
1. **Kali 环境缺少 locale 文件** - glibc 反复尝试加载导致延迟
2. ~~perm_config 初始化问题~~ - 未使用 --perm-config 参数时不触发
3. ~~temp.c 路径探测逻辑~~ - 已改为硬编码，无性能问题

**待验证**:
- 在 Kali 环境中生成 locale 是否解决延迟
- 对比原二进制文件和修改版本的系统调用差异

## 关键决策记录

### 1. 使用 musl libc 替代 glibc
**理由**: Android seccomp 过滤器拦截特定系统调用，glibc 启动时会触发这些调用导致崩溃。musl 更加精简，避免了这些问题。

### 2. 静态链接
**理由**: 
- 无 root 环境无法安装共享库
- 避免依赖 Termux 环境
- 单一二进制文件便于部署

### 3. 硬编码临时目录路径
**理由**: 
- 动态探测逻辑复杂且不可靠
- Termux 的 PREFIX 环境变量通常已设置
- 硬编码路径更可靠

### 4. 保留 fake_id0 原有逻辑
**理由**: 
- 原有 fake_id0 扩展已经过充分测试
- 新 perm_config 功能是增量添加，不影响原有逻辑
- 用户未使用 --perm-config 参数时行为与原版一致

## 未解决问题

1. **启动延迟 5 秒** - 可能是 Kali 环境 locale 缺失导致
2. **SIGINT 终止原因** - 需要进一步分析是否为用户手动中断

## 相关工具

- **交叉编译工具链**: `/tmp/aarch64-linux-musl-cross/`
- **musl talloc**: `/tmp/musl-talloc-aarch64/libtalloc.a`
- **构建脚本**: 待创建自动化脚本
