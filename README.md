# clogger

一个功能强大、高性能的C语言日志库，支持多平台、多线程环境下的日志记录。

[![Version](https://img.shields.io/badge/version-2025.12.12-blue.svg)](VERSION)
[![License](https://img.shields.io/badge/license-MapAware%20License%20v1.0-green.svg)](COPYRIGHT)
[![Author](https://img.shields.io/badge/author-cheungmine%40qq.com-orange.svg)](AUTHOR)

## 目录

- [简介](#简介)
- [特性](#特性)
- [安装](#安装)
- [快速开始](#快速开始)
- [配置](#配置)
- [API参考](#api参考)
- [log4tcl](#log4tcl)
- [常见问题](#常见问题)
- [贡献](#贡献)

## 简介

clogger是一个为C语言应用程序设计的日志库，提供了灵活、高效的日志记录功能。它支持多种日志输出方式、多级日志控制、日志文件滚动、跨平台使用等特性，适用于各种规模的应用程序开发。

## 特性

- **多平台支持**：支持Linux、Windows、Mingw、Cygwin等多种操作系统
- **多线程安全**：内部实现了线程安全机制，可在多线程环境中安全使用
- **多种日志输出**：支持标准输出、系统日志、滚动文件、共享内存等多种输出方式
- **日志级别控制**：支持TRACE、DEBUG、INFO、WARN、ERROR、FATAL等多个日志级别
- **日志文件滚动**：支持基于时间和文件大小的日志滚动策略
- **灵活的配置**：通过配置文件可以灵活控制日志行为
- **丰富的格式选项**：支持多种日期格式、时间精度、颜色样式等
- **TCL绑定**：提供log4tcl模块，支持在TCL脚本中使用clogger

## 安装

### 编译clogger

#### Linux/Unix系统

```bash
# 清理并编译
$ make clean && make

# 安装 (默认安装到/usr/local)
$ sudo make install

# 安装到指定目录
$ sudo make INSTALL_PREFIX=/path/to install

# 卸载
$ sudo make uninstall
```

#### Windows系统

1. 使用Visual Studio打开`msvc/clogger.sln`
2. 选择合适的配置（Debug/Release, x86/x64）
3. 构建解决方案

### 配置文件

clogger会按照以下顺序查找配置文件`clogger.cfg`：
1. 应用程序二进制文件所在目录
2. 应用程序二进制文件所在目录的`conf/`子目录
3. 应用程序二进制文件所在目录的上级目录的`conf/`子目录
4. 环境变量`$CLOGGER_CFG_DIR/`指定的目录
5. Windows: `C:\TEMP\clogger\` 或 Linux: `/etc/clogger/`

## 快速开始

### 基本使用示例

```c
#include <clogger/clogger_helper.h>

int main()
{
    // 初始化日志管理器
    const char* idents[] = {"myapp", NULL};
    logger_manager_init(NULL, idents);

    // 获取日志记录器
    clog_logger logger = logger_manager_load("myapp");

    // 记录不同级别的日志
    CLOG_TRACE(logger, "[%d:%lld] clogger is a high-performance, reliable, threads safety, easy to use, pure C logging library.", tid, count);
    CLOG_DEBUG(logger, "[%d:%lld] As far as I know in the C world there was NO perfect logging facility for applications like logback in  java or log4cxx in c++.", tid, count);
    CLOG_INFO(logger, "[%d:%lld] Using printf can work, but can not be redirected or reformatted easily.", tid, count);
    CLOG_WARN(logger, "[%d:%lld] syslog is slow and is designed for system use.", tid, count);
    CLOG_ERROR(logger, "[%d:%lld] Others like LOG4C(has BUGs) or ZLOG(over-design) is somewhat of complication.", tid, count);
    CLOG_FATAL(logger, "[%d:%lld] So I wrote CLOGGER from the bottom up!", tid, count);

    // 清理资源
    logger_manager_uninit();

    return 0;
}
```

### 编译和运行

```bash
# 编译
gcc -o myapp myapp.c -lclogger -lpthread

# 运行
./myapp
```

## 配置

clogger通过配置文件控制其行为。以下是配置文件的主要部分：

### 基本配置

```ini
[clogger:myapp]
    # 标识符的数字密钥，不超过9位数
    magickey = 350137278

    # 消息最大字节数
    maxmsgsize = 32768

    # 环形缓冲区队列长度
    queuelength = 1024

    # 日志输出目标，可以是以下组合:
    #   STDOUT - 标准输出
    #   SYSLOG - 系统日志
    #   ROFILE - 滚动文件
    #   SHMLOG - 共享内存
    appender = STDOUT,ROFILE,SHMLOG

    # 日志文件目录（当appender包含ROFILE时）
    # 注意：指定的目录必须存在，否则不会创建日志文件！
    pathprefix = /tmp/clogger/<IDENT>

    # 日志文件名模式
    nameprefix = <IDENT>.<DATE>.log

    # 滚动策略
    rollingpolicy = timesizepolicy

    # 日志级别: TRACE, DEBUG, INFO, WARN, ERROR, FATAL
    loglevel = INFO

    # 布局类型: PLAIN 或 DATED
    layout = DATED

    # 日期格式
    dateformat = RFC-3339

    # 时间精度: s(秒), ms(毫秒), us(微秒)
    timeunit = ms

    # 启用标志
    autowrapline     # 自动换行
    localtime        # 使用本地时间
    timestampid      # 使用纳秒作为消息唯一ID
    colorstyle       # 启用颜色样式输出
    filelineno       # 打印文件名和行号
    function         # 打印函数名
    processid        # 打印进程ID
    threadno         # 打印线程ID
```

### 滚动策略配置

```ini
[rollingpolicy:timesizepolicy]
    # 时间策略: 1min,5min,10min,30min,hour,day,mon,year
    rollingtime = 5min

    # 单个文件最大大小
    maxfilesize = 80M

    # 最大文件数量
    maxfilecount = 30
```

## API参考

### 日志管理器API

- `logger_manager_init()` - 初始化日志管理器
- `logger_manager_uninit()` - 反初始化日志管理器
- `get_logger_manager()` - 获取日志管理器实例
- `logger_manager_load()` - 加载指定标识符的日志记录器
- `logger_manager_get()` - 通过索引获取日志记录器

### 日志记录器API

- `clog_logger_get_level()` - 获取日志级别
- `clog_logger_set_level()` - 设置日志级别
- `clog_logger_level_enabled()` - 检查日志级别是否启用
- `clog_logger_log_message()` - 记录消息
- `clog_logger_log_format()` - 格式化记录消息

## log4tcl

log4tcl是clogger的TCL语言绑定，允许在TCL脚本中使用clogger功能。

### 编译log4tcl

#### Linux/Unix系统

```bash
$ cd src/log4tcl
$ make clean && make
$ sudo make install
```

#### Windows系统 (MINGW64)

1. 安装TCL开发包:
   ```bash
   MSYS2 > $ pacman -S mingw-w64-x86_64-tcl
   ```

2. 编译log4tcl:
   ```bash
   MINGW64 > $ cd src/log4tcl
   MINGW64 > $ make clean && make
   ```

### log4tcl使用示例

```tcl
# 加载log4tcl包
package require log4tcl

# 初始化日志
log4tcl::init myapp INFO

# 记录日志
log4tcl::info myapp "应用程序启动"
log4tcl::error myapp "发生错误: 文件未找到"

# 清理
log4tcl::cleanup
```

## 常见问题

### 禁止广播日志消息

在某些系统上，日志消息可能会广播给所有用户。要禁用此功能：

1. 编辑系统日志配置文件:
   ```bash
   sudo vi /etc/systemd/journald.conf
   ```

2. 修改以下设置:
   ```ini
   # 禁止向控制台转发消息
   ForwardToConsole=no

   # 禁止向所有登录用户广播消息
   ForwardToWall=no

   # 禁止向内核日志缓冲区转发消息
   ForwardToKMsg=no
   ```

3. 重启系统日志服务:
   ```bash
   sudo systemctl restart systemd-journald
   ```

## 贡献

欢迎为clogger项目做出贡献！如果您有好的想法、发现了bug或有改进建议，请通过以下方式联系：

- 邮箱: cheungmine@qq.com
- 项目主页: https://github.com/pepstack/clogger

## 许可证

clogger使用MapAware License v1.0许可证。详见[COPYRIGHT](COPYRIGHT)文件。

## 当前版本

- 2025.12.12
