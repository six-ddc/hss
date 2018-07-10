# hss

[![Release](http://github-release-version.herokuapp.com/github/six-ddc/hss/release.svg?style=flat)](https://github.com/six-ddc/hss/releases/latest)
[![Build Status](https://travis-ci.org/six-ddc/hss.svg?branch=master)](https://travis-ci.org/six-ddc/hss)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](https://opensource.org/licenses/MIT)

[简体中文README](README-zh.md)

## What's hss?

`hss` is an interactive ssh client for multiple servers. It will provide almost the same experience as in the bash environment. It supports:

* interactive input: based on [libreadline](https://cnswww.cns.cwru.edu/php/chet/readline/rltop.html).
* history: responding to the `C-r` key.
* auto-completion: completion from remote server on the `tab` key, for commands and paths.

Command is executed on all servers in parallel. Execution on one server does not need to wait for that on another server to finish before starting. So we can run a command on hundreds of servers at the same time.

## A quick start

```
Usage: hss [-f hostfile] [-o file] [-u username] [command]

Options:
  -f, --file=FILE           file with the list of hosts or - for stdin
  -H, --host                specifies a host option, support the same options as the ssh command
  -c, --common              specify the common ssh options (i.e. '-p 22 -i identity_file')
  -u, --user                the default user name to use when connecting to the remote server
  -o, --output=FILE         write remote command output to a file
  -v, --verbose             be more verbose
  -V, --version             show program version
  -h, --help                display this message
```

* This is a screenshot

[![asciicast](https://asciinema.org/a/78W5h0su6C5M8pafyqkUfBaTv.png)](https://asciinema.org/a/78W5h0su6C5M8pafyqkUfBaTv)

## How to install it?

* MacOS

```bash
brew install hss
```

* Linux

    * Install dependency

    ```bash
    ## on CentOS
    yum install readline-devel

    ## on Ubuntu / Debian 
    apt-get install libreadline6-dev
    ```

    * Compile and install

    ```bash
    make && make install
    ```

* Or you can download the binary release [here](https://github.com/six-ddc/hss/releases) .

## How to use it?

The fundamental of `hss` is to execute the `ssh` command for every `host`, and then show the results on the terminal. So `hss` supports every argument supported by the `ssh` command. Following is an example of the `hostfile`:

```
192.168.1.1
-p 2222 root@192.168.1.2
-p 2222 -i ~/.ssh/identity_file root@192.168.1.3
-p 2222 -oConnectTimeout=3 root@192.168.1.4
```

Connect to servers:

```
# Specify the hostfile directly
hss -f hostfile

# Or pass servers in arguments
hss -H '192.168.1.1' -H '-p 2222 root@192.168.1.2' -H '-p 2222 -i ~/.ssh/identity_file root@192.168.1.3' -H '-p 2222 -oConnectTimeout=3 root@192.168.1.4'
```

Passthrough of `ssh` arguments are supported. For example, by specify `-c '-oConnectTimeout=3'`, sessions without a timeout configured will set its timeout on this argument.

### Usage of readline

The interactive input is implemented on `libreadline`, supporting command and path completion from remote, history storage and searching, moving around, etc.

* Command history is stored in file `~/.hss_history`.
* Completion of commands and paths are based on the first server in the list.
* Path completion is available when the first input character is `/`, `~` or `.`.
* The path completion will end with an `@` character for directory symbol-links. (We haven't found any solution to solve it yet)

Following is a list of quick commands (please refer to [readline](http://cnswww.cns.cwru.edu/php/chet/readline/readline.html#SEC1) for more)

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

## Goals of the future versions

* [ ] Solve the "@" suffix problem on directory symbol-link.

