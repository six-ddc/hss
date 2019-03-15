# hss

[![Release](http://github-release-version.herokuapp.com/github/six-ddc/hss/release.svg?style=flat)](https://github.com/six-ddc/hss/releases/latest)
[![Build Status](https://travis-ci.org/six-ddc/hss.svg?branch=master)](https://travis-ci.org/six-ddc/hss)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](https://opensource.org/licenses/MIT)

## 概述

hss是一款可交互式的ssh批量执行命令的客户端，交互输入模式基于[libreadline](https://cnswww.cns.cwru.edu/php/chet/readline/rltop.html)实现，使你能像操作bash一样地输入需要执行的命令，同时也支持命令搜索，历史命令纪录等。并且工具支持在输入命令时，按一下`tab`键，即可根据远程服务器的信息，进行文件路径和执行命令补全。另外命令的执行是异步的，无需等待一台机器命令执行完成才执行下一台的ssh操作，可支持同时操作数百台服务器。

## 预览

```
Usage: hss [-f hostfile] [-o file] [-u username] [command]...

Options:
  -f file        file with the list of hosts
  -H host        specifies a host option, support the same options as the ssh command
  -l limit       number of multiple ssh to perform at a time (default: unlimited)
  -u user        the default user name to use when connecting to the remote server
  -c opts        specify the common ssh options (i.e. '-p 22 -i identity_file')
  -o file        write remote command output to a file
  -i             force use a vi-style line editing interface
  -v             be more verbose
  -V             show program version
  -h             display this message

For more information, see https://github.com/six-ddc/hss
```

* 使用效果图如下：

[![asciicast](https://asciinema.org/a/233954.svg)](https://asciinema.org/a/233954)

## 安装

* MacOS

```bash
brew install hss
```

* Linux

    * 安装依赖

    ```bash
    ## on CentOS
    yum install readline-devel

    ## on Ubuntu / Debian 
    apt-get install libreadline6-dev
    ```

    * 编译&安装

    ```bash
    make && make install
    ```

* 或者直接下载[Release文件](https://github.com/six-ddc/hss/releases)

## 指南

hss的实现原理是对每个host，直接调用本地的`ssh`命令去执行服务器操作，然后再通过进程间通信将执行结果返回给终端。

故此hss支持所有的`ssh`命令的参数选项。以下是hostfile示例文件：

```
192.168.1.1
-p 2222 root@192.168.1.2
-p 2222 -i ~/.ssh/identity_file root@192.168.1.3
-p 2222 -oConnectTimeout=3 root@192.168.1.4
```

连接上述机器的命令如下：

```
# 指定配置文件的方式
hss -f hostfile

# 通过传参的方式
hss -H '192.168.1.1' -H '-p 2222 root@192.168.1.2' -H '-p 2222 -i ~/.ssh/identity_file root@192.168.1.3' -H '-p 2222 -oConnectTimeout=3 root@192.168.1.4'
```

hss命令本身也支持透传参数到ssh命令，，比如指定了`-c '-oConnectTimeout=3'`，那么对于没有配置超时时间的，将用该值作为超时设置。

### readline使用

可交互式的命令输入，基于`libreadline`实现，支持远程命令补全，远程文件路径补全，历史命令保存，历史命令搜索，快捷移动等。（更多命令参考[readline说明](http://cnswww.cns.cwru.edu/php/chet/readline/readline.html#SEC1)）

* 输入的历史命令保存在`~/.hss_history`文件中
* 远程命令和文件路径补全，数据信息来自于第一个host连接
* 进行文件路径补全时，需要当前单词的首字符是`/`, `~`, `.`才可提示补全

## 下版本方向

* [x] 解决链接目录补全显示@结尾的问题


