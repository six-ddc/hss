# hss

[![Build Status](https://travis-ci.org/six-ddc/hss.svg?branch=master)](https://travis-ci.org/six-ddc/hss)

## 概述

hss是一款可交互式的ssh批量登录客户端，工具基于libreadline，使你能像操作bash一样输入需要执行的命令。并且工具支持在输入命令时，支持按一下tab键，即可根据远程服务器的信息，进行文件路径和执行命令补全，就像是操作本地一样顺滑。另外命令的执行是异步的，无需等待一台机器命令执行完成才执行下一台的ssh操作，可支持同时操作数百台服务器。

hss还支持插件扩展，可通过`Esc`键将运行模式从`remote`切换到`inner`，在这里可处理一些批量操作：动态增加删除机器，设置程序运行时的配置等，更多的功能可能将在后续版本逐渐添加。

## 预览

```
Usage: hss [-f hostfile] [-o file] [-u username] [command]

Options:
  -f, --file=FILE           file with the list of hosts or - for stdin
  -H, --host                specifies a host option, support the same options as the ssh command
  -i, --identity-file=FILE  specifies a default identity (private key) authentication file
  -u, --user                the default user name to use when connecting to the remote server
  -t, --conn-timeout        ssh connect timeout (default 30 sec)
  -o, --output=FILE         write remote command output to a file
  -v, --verbose             be more verbose (i.e. show ssh command)
  -V, --version             show program version
  -h, --help                display this message

For more information, see https://github.com/six-ddc/hss
```

![](https://github.com/six-ddc/hss/blob/master/demo.gif?raw=true)

## 安装

* 安装依赖

```
## on MacOS
brew install readline
## on CentOS
apt-get install libreadline6 libreadline6-dev
## on Ubuntu / Debian 
yum install readline-devel
```

* 编译

```
make && make install
```

* 或者直接下载[Release文件](https://github.com/six-ddc/httpflow/releases)

## 指南


