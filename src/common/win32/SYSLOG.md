## syslog for win32

2025-08-23

用于在 Windows 机器上向运行 syslog 服务端的服务器发送日志信息。

syslog-client.c 仅仅运行在 Windows 上。

### syslog 服务端

syslog 服务端是运行 syslog 服务的计算机。比如 Ubuntu。(假设: ip=192.168.56.10，端口=514)

启用 Ubuntu syslog 远程服务：

  - 修改/etc/rsyslog.conf (去掉注释)

    module(load="imudp")
    input(type="imudp" port="514")

  - 重启服务

    sudo systemctl restart rsyslog

检查端口是否监听成功：

    sudo netstat -tulnp | grep 514

    # 预期输出应包含rsyslogd和UDP 514

若需远程接收日志，建议配合防火墙规则

关闭UDP端口（注释配置后重启）

    sudo systemctl restart rsyslog

验证端口释放

    sudo ss -ulnp | grep 514

以上确保 syslog 日志服务启用。

### syslog Windows 客户端

创建配置文件：syslog.host。只一行：

    192.168.56.10:514

创建系统环境变量 SYSLOG_CONF_DIR：

    SYSLOG_CONF_DIR=C:/var/log

这个也是默认值。你可以配置成不同的值。

以下文件必须存在：

    $SYSLOG_CONF_DIR/syslog.host

如果程序编译时指定了 SYSLOG_CONF_DIR 的值，则忽略其他配置，例如：

    -DSYSLOG_CONF_DIR="D:/somewhere/myapp"
