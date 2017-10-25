# hss

[![Release](http://github-release-version.herokuapp.com/github/six-ddc/hss/release.svg?style=flat)](https://github.com/six-ddc/hss/releases/latest)
[![Build Status](https://travis-ci.org/six-ddc/hss.svg?branch=master)](https://travis-ci.org/six-ddc/hss)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](https://opensource.org/licenses/MIT)

## 概述

hss是一款可交互式的ssh批量执行命令的客户端，交互输入模式基于[libreadline](https://cnswww.cns.cwru.edu/php/chet/readline/rltop.html)实现，使你能像操作bash一样地输入需要执行的命令，同时也支持命令搜索，历史命令纪录等。并且工具支持在输入命令时，按一下`tab`键，即可根据远程服务器的信息，进行文件路径和执行命令补全。另外命令的执行是异步的，无需等待一台机器命令执行完成才执行下一台的ssh操作，可支持同时操作数百台服务器。

hss还支持插件扩展，可通过`Esc`键将运行模式从`remote`切换到`inner`，在这里可处理一些批量操作：批量上传下载文件、动态增加删除机器、设置程序运行时的配置等，更多的有趣的功能可能将在后续版本逐渐添加。

## 预览

```
Usage: hss [-f hostfile] [-o file] [-u username] [command]

Options:
  -f, --file=FILE           file with the list of hosts or - for stdin
  -H, --host                specifies a host option, support the same options as the ssh command
  -c, --common              specify the common ssh options (i.e. '-p 22 -i identity_file')
  -u, --user                the default user name to use when connecting to the remote server
  -o, --output=FILE         write remote command output to a file
  -l, --local               enable local running mode
  -v, --verbose             be more verbose
  -V, --version             show program version
  -h, --help                display this message
```

* 使用效果图如下：

![](https://github.com/six-ddc/hss/blob/master/demo.gif?raw=true)

## 安装

* 安装依赖

```bash
## on MacOS
brew install readline

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

# 管道方式，这里必须指定需要执行的命令
cat hostfile | hss -f - 'date'

# 通过传参的方式
hss -H '192.168.1.1' -H '-p 2222 root@192.168.1.2' -H '-p 2222 -i ~/.ssh/identity_file root@192.168.1.3' -H '-p 2222 -oConnectTimeout=3 root@192.168.1.4'
```

hss命令本身也支持透传参数到ssh命令，，比如指定了`-c '-oConnectTimeout=3'`，那么对于没有配置超时时间的，将用该值作为超时设置。

### inner模式

通过`Esc`可将运行模式从默认的`remote`切换到`inner`，inner模式下支持的命令都是程序内部实现的（可参考[command目录](https://github.com/six-ddc/hss/tree/master/command)），目前支持以下几种：

* help

    列出inner命令列表

* upload

    ```
    Usage: upload <local_path> <remote_path>
    ```

    将本地文件上传到各个服务器对应路径

* download

    ```
    Usage: download <remote_path> <local_path>
    ```

    将各个服务器指定路径的文件下载到本地（最终每个下载的本地文件都将追加host后缀以作区分）

* config

    ```
    Usage: config <command>

    Commands:
      get    all|<config>       : get config
      set    <config> [value]   : set config

    Config:
      output <filename>         : redirect output to a file. stdout is used if filename is '-'
      common-options <filename> : common ssh options

    ```

    配置管理，可get/set程序运行的一些配置，比如可通过`config set output a.txt`，将后面remote模式下的命令执行结果都重定向输出到a.txt文件中，需要重新输出到终端，则使用`config set output`复原

* host

    ```
    Usage: host <command>

    Commands:
      list               : list all ssh slots
      add <ssh_options>  : add a ssh slot
      del <ssh_host>     : delete special ssh slot
    ```

    host管理，可动态增加或删除需要连接的远程host

### readline使用

可交互式的命令输入，基于`libreadline`实现，支持远程命令补全，远程文件路径补全，历史命令保存，历史命令搜索，快捷移动等

* 输入的历史命令保存在`~/.hss_history`文件中
* 远程命令和文件路径补全，数据信息来自于第一个host连接
* 进行文件路径补全时，需要当前单词的首字符是`/`, `~`, `.`才可提示补全
* 对于是符号链接的目录，路径补全时会提示以`@`结尾，目前还没找到好的解决办法

以下列举一些简单的快捷命令（更多命令参考[readline说明](http://cnswww.cns.cwru.edu/php/chet/readline/readline.html#SEC1)）

```
C-a       Move to the start of the line.
C-e       Move to the end of the line.
M-f       Move forward a word, where a word is composed of letters and digits.
M-b       Move backward a word.

C-l       Clear the screen, reprinting the current line at the top. 
C-u       Kill backward from the cursor to the beginning of the current line.
C-w       Kill the word behind point, using white space as a word boundary.

C-r       Search backward starting at the current line and moving up through the history as necessary.
```

## 下版本方向

* [ ] inner多层级命令补全
* [ ] upload/download文件路径补全
* [ ] 解决链接目录补全显示@结尾的问题
