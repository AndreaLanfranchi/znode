# Command line arguments

- [Documents Index](README.md)
- [Usage](#usage)
- [--help](#--help)
- [--data-dir](#--data-dir)
- [Chaindata arguments](#chaindata)
    - [--chaindata.exclusive](#--chaindataexclusive)
    - [--chaindata.readahead](#--chaindatareadahead)
    - [--chaindata.maxsize](#--chaindatamaxsize)
    - [--chaindata.growthsize](#--chaindatagrowthsize)
    - [--chaindata.pagesize](#--chaindatapagesize)
- [Network arguments](#network)
    - [--network.ipv4only](#--networkipv4only)
    - [--network.maxactiveconnections](#--networkmaxactiveconnections)
    - [--network.maxconnectionsperip](#--networkmaxconnectionsperip)
    - [--network.handshaketimeout](#--networkhandshaketimeout)
    - [--network.inboundtimeout](#--networkinboundtimeout)
    - [--network.outboundtimeout](#--networkoutboundtimeout)
    - [--network.idletimeout](#--networkidletimeout)
    - [--network.pinginterval](#--networkpinginterval)
    - [--network.pingtimeout](#--networkpingtimeout)
    - [--network.connect](#--networkconnect)
    - [--network.connecttimeout](#--networkconnecttimeout)
    - [--network.forcednsseed](#--networkforcednsseed)
- [--etl.buffersize](#--etlbuffersize)
- [Syncloop arguments](#syncloop)
    - [--syncloop.batchsize](#--syncloopbatchsize)
    - [--syncloop.throttle](#--syncloopthrottle)
    - [--syncloop.loginterval](#--synclooploginterval)
- [--fakepow](#--fakepow)
- [--zk.nochecksums](#--zknochecksums)

## Usage

Executing the node instance is, in most of the cases, pretty straight forward.
All command line arguments are already set to default values we have found to be the most efficient.
As a result you can just run the following command to start the node:

```bash
$ ./<build_dir>/zenpp
```

where `<build_dir>` is the directory where the node was built.
If you have downloaded a pre-built binary, you can just run `zenpp` from the directory where it was extracted.

_For Windows users the binary is named `zenpp.exe`_

However, if you want to change some of the default values, you have always the option to pass them as command line
arguments.
To see the full list of available arguments, you can run the following command:

```bash
$ ./zenpp --help
```

An argument name is always preceded by two dashes (`--`) and an explicative name. When argument names contain dots (`.`)
they indicate the are related to a scope.
For example all arguments related to the settings for chaindata db parameters are prefixed with `--chaindata.`.
Arguments are divided in two categories:

- **Flags**: arguments which are false by default and only if explicitly set in command line become true. For
  example `--chaindata.readahead` is a flag.
- **Options**: arguments which have a default value and can be changed by the user. For example `--data-dir` is an
  option.

Arguments can be passed in two ways:

- **'=' form**: `--<argument-name>=<argument-value>`
- **space form**: `--<argument-name> <argument-value>`

Arguments specifying `size` values can be expressed in two ways:

- **absolute**: where the number is fully expressed. For example to indicate a value of 2 kilo bytes we'd use the
  form `--<argument-name>=2048`
- **suffixed**: where the number is expressed as a multiplier of a suffix. For example to indicate a value of 2 kilo
  bytes we'd use the form `--<argument-name>=2KiB`

Accepted suffixes are:

- `KiB`, `MiB`, `GiB`, `TiB` for [binary multiples](https://en.wikipedia.org/wiki/Binary_prefix) of bytes. (i.e. 5
  KiB == (5*2**10) == 5'120 bytes)
- `KB`, `MB`, `GB`, `TB` for decimal multiples of bytes. (i.e. 5 KB == (5*10**3) 5'000 bytes)

It goes without saying that the suffixed form is more readable and recommended.

Here we analyze the usage and meaning of each argument.

## `--help`

This argument prints the list of available arguments and their meaning.
When this argument is detected `zend` will print the list **and exit** regardless any other argument.

## `--data-dir`

This argument specifies the directory where the node will store all the data and/or expects some useful data to be
found.
By default the value of datadir is OS dependent and is determined in this way:

- Windows: `%LOCALAPPDATA%\.zenpp` (aka `%APPDATA%\Local\.zenpp`)
- Linux: `$HOME/.local/share/.zenpp`
- Mac: `$HOME/Library/Application Support/.zenpp`
  The structure of the zen directory is the following:

```
<root> (usually ~/.local/share/.zenpp)
├── chaindata    // where the blockchain data (in MDBX format) is stored
├── etl-tmp      // where temporary ETL files are stored
├── nodes        // where the nodes information (in MDBX format) is stored
├── ssl-cert     // where the SSL certificate(s) are stored
└── zk-params    // where the zkSNARK parameters are stored
```

Data directory (and all subdirs) is created automatically if it does not exist.
It is expected that the user has read/write permissions on the data directory.
Consider also that the chaindata directory can grow up to hundreds GiB of data (on behalf of the network you're
connecting to) and, overall, the space required by data directory might require up to 300GiB:
please ensure that you have enough space on the partition where the data directory is located (generally the primary
one).
If you don't have enough space there, or simply want to have the data directory positioned elsewhere, you can always
change the location of the data directory path by passing the `--data-dir` argument such as:

```bash
$ ./zenpp [...] --data-dir=/path/to/parent/dir
```

where `/path/to/parent/dir` is the path to the directory where you want to store all the data handled by the
application.
The argument can also be specified as relative to the directory of the `zenpp` binary:

```bash
$ ./zenpp [...] --data-dir=../<whatever>
```

**Warning. `zenpp` is a quite intensive IO application and it is recommended to have access to a storage directory on a
directly attached support. For decent performance SSD is recommended. NVMe is even better. Avoid usage of network
attached supports: IO throughput is not as important as access latency (a lot of random accesses are performed on the
database).
If you intend to install and run `zenpp` on an AWS instance be advised that using gp2/gp3 storage performances might
result disappointing**

## Chaindata

In this section we analyze all the arguments related to the chaindata db parameters.

### `--chaindata.exlusive`

This argument is a flag. When set the chaindata db is opened in exclusive mode: this means that only `zenpp` process can
access the chaindata db.

### `--chaindata.readahead`

This argument is a flag. When set the MDBX engine will try to read and cache more data than actually required.
On some systems this can improve the performance of the node but your mileage may vary.

### `--chaindata.maxsize`

This argument is an option. It specifies the maximum size of the chaindata db in bytes.
When the size of the database reaches this threshold, the node won't be able to store new data and will stop due to
errors.
However you can restart the node setting a larger value if you have space available.
The default value is 1TiB. If you want to change this value you can pass the argument such as:

```bash
$ ./zenpp [...] --chaindata.maxsize=2TiB
```

where `2TiB` is the new maximum size of the chaindata db you're willing to allow.

### `--chaindata.growthsize`

This argument is an option. It specifies the increment size of the chaindata db in bytes.
The default value is 2GiB which also corresponds to the initial size of the database. Wheneveer MDBX engine needs more
space it will extend the database file by this amount of bytes.
If you want to change this value you can pass the argument such as:

```bash
$ ./zenpp [...] --chaindata.growthsize=4GiB
```

**Warning. The extension of the database file is a costly operation: in particular on Windows it requires the whole
memory remapping of the database. Don't set this value to small amounts, especially during initial syncs from scratch,
to not impair performance on more frequent extensions**

### `--chaindata.pagesize`

This argument is an option. It specifies the page size MDBX has to use.

**Important: this value MUST be a power of 2**

_MDBX segments its database file in pages where each page has the size here specified.
MDBX's databases can have very large data sizes but are capped to a maximum 2147483648 (0x80000000ULL) data pages, hence
the maximum size a database file can reach is a function of pagesize.
The min value is 256B (which translates into a max db file size of 512GiB) while the max value is 65KiB (which
translates into a max db file size of 128TiB)_

The default value is 4KiB which gives room up to 8TiB of data **and is also the default page size for memory allocation
by modern OSes**.
If you want to change this value you can pass the argument such as:

```bash
$ ./zenpp [...] --chaindata.pagesize=8KiB
```

where `8KiB` is the new page size you're willing to use.

## Network

In this section we analyze all the arguments related to the networking parameters.

### `--network.ipv4only`

This argument is a flag. When set the node will limit network activity to IPv4 addresses only.
To enable the limitation you can pass the argument such as:

```bash
$ ./zenpp [...] --network.ipv4only
```

### `--network.maxactiveconnections`

This argument is an option. It specifies the maximum number of active concurrent connections the node can have at the
same time.
The default value is 256. Acceptable values are in range (32, 256). If you want to change this value you can pass the
argument such as:

```bash
$ ./zenpp [...] --network.maxactiveconnections 64
```

where `64` is the new maximum number of active concurrent connections you're willing to allow.
Please note that in any case at least 16 connections are reserved to the "outbound" bucket or, in other words, the node
will always keep at least 16 connections to dial-out to other nodes.
The remaining connections can be used to accept incoming (dial-in) connections from other nodes.

### `--network.maxconnectionsperip`

This argument is an option. It specifies the maximum number of concurrent connections the node can have with a single
peer from the same IP address.
The default value is 1. Acceptable values are in range (1, 16). If you want to change this value you can pass the
argument such as:

```bash
$ ./zenpp [...] --network.maxconnectionsperip 2
```

where `2` is the new maximum number of concurrent connections you're willing to allow with a single peer from the same
IP address.
It is advisable not to change this value unless you have a very good reason to do so.

### `--network.handshaketimeout`

This argument is an option. It specifies the maximum amount of time, in seconds, the node waits for a peer to complete
the handshake process.
The default value is 10. Acceptable values are in range (5, 30). If you want to change this value you can pass the
argument such as:

```bash
$ ./zenpp [...] --network.handshaketimeout 15
```

where `15` is the new maximum amount of time, in seconds, the node waits for a peer to complete the handshake process.
Should a peer fail to complete the handshake process within the specified timeout, the connection is closed.
Note: the handshake process is the initial phase of the connection where the two peers exchange information about their
capabilities and the node verifies the peer's identity.

### `--network.inboundtimeout`

This argument is an option. It specifies the maximum amount of time, in seconds, an inbound message can take to be fully
received. Measurement starts when a message header is received.
The default value is 10. Acceptable values are in range (5, 30). If you want to change this value you can pass the
argument such as:

```bash
$ ./zenpp [...] --network.inboundtimeout 15
```

where `15` is the new maximum amount of time, in seconds, an inbound message can take to be fully received.
Should a message fail to be fully received within the specified timeout, the connection is closed. This is done to
prevent Slowloris attacks.

### `--network.outboundtimeout`

This argument is an option. It specifies the maximum amount of time, in seconds, an outbound message can take to be
fully written on the socket. Measurement starts when the very first byte of the message is sent.
The default value is 10. Acceptable values are in range (5, 30). If you want to change this value you can pass the
argument such as:

```bash
$ ./zenpp [...] --network.outboundtimeout 15
```

where `15` is the new maximum amount of time, in seconds, an outbound message can take to be fully transmitted.
Should a message fail to be fully transmitted within the specified timeout, the connection is closed. This is done to
prevent Slowloris attacks.

### `--network.idletimeout`

This argument is an option. It specifies the maximum amount of time, in seconds, a connection can stay idle before being
closed.
The default value is 300. Acceptable values are in range (30, 3600). If you want to change this value you can pass the
argument such as:

```bash
$ ./zenpp [...] --network.idletimeout 600
```

where `600` is the new maximum amount of time, in seconds, a connection can stay idle before being closed.
Note : a connection is deemed _idle_ when no data is exchanged between the two peers. Data exchange **DOES NOT INCLUDE
** `ping` and `pong` messages which are evaluated only to measure the average latency of the connection.

### `--network.pinginterval`

This argument is an option. It specifies the get_interval, in seconds, between `ping` messages the node sends out to
connected peers to evaluate communication latency.
The default value is 120. Acceptable values are in range (30, 3600). If you want to change this value you can pass the
argument such as:

```bash
$ ./zenpp [...] --network.pinginterval 240
```

where `240` is the new get_interval, in seconds, between `ping` messages the node sends out to connected peers to evaluate
communication latency.
Note: the effective get_interval between `ping` messages is randomized, on a connection basis, in a range value +/- 30%.
This is done to avoid predictability of get_interval and as a result a possible _cheat_ on response values.

### `--network.pingtimeout`

This argument is an option. It specifies the maximum amount of time, in milliseconds, the node waits for a `pong` reply
in response to a `ping` message.
The default value is 500. Acceptable values are in range (100, 5000). If you want to change this value you can pass the
argument such as:

```bash
$ ./zenpp [...] --network.pingtimeout 1000
```

where `1000` is the new maximum amount of time, in milliseconds, the node waits for a `pong` reply in response to
a `ping` message.
Should a node fail to reply to a `ping` message within the specified timeout, the connection is closed. This helps to
maintain a healthy network and avoid to waste resources on unresponsive peers.

### `--network.connect`

This argument is an option. It specifies a user defined list of peers' endpoints the node will try to connect to at
startup.
The default value is empty which means the node will try to connect to already known nodes (if any) or to get a list of
available nodes from DNS seeds.
To specify an endpoint you can pass the argument such as:

```bash
$ ./zenpp [...] --network.connect=[<ip>:<port> <ip>:<port>,...]
```

where `<ip>` is the IP address of the peer and `<port>` is the port number the peer is listening to.
You can specify as many endpoints as you want, separated by a space.
The syntax to specify an endpoint is flexible and you can use the following forms:

- `<ipv4>`: the port will be derived by the network type (mainnet, testnet, regtest)
- `<ipv4>:<port>`: the port will be the one specified
- `<ipv6>`: the port will be derived by the network type (mainnet, testnet, regtest)
- `[<ipv6>]:<port>`: the port will be the one specified. Note that in case of IPv6 when the port is specified the
  address MUST be enclosed in square brackets

### `--network.connecttimeout`

This argument is an option. It specifies the maximum amount of time, in seconds, the node waits for a dial-out
connection to be successfully established.
The default value is 2. Acceptable values are in range (1, 5). If you want to change this value you can pass the
argument such as:

```bash
$ ./zenpp [...] --network.connecttimeout 3
```

where `3` is the new maximum amount of time, in seconds, the node waits for a dial-out connection to be successfully
established.
Note: This value is used only when the node is trying to connect to a peer and not when a peer is trying to connect to
the node.
Note: The connection might time out earlier than this value if the underlying OS decides so.

### `--network.forcednsseed`

This argument is a flag. When set the node will try to get a list of available nodes from a hardcoded list of host seeds
to be resolved by DNS.
Note : regardless this flag is set or not, the node will always try to get a list of available nodes from DNS seeds when
no other nodes addresses are known: this means no manually defined nodes have been specified with `--network.connect`
and no nodes have been discovered in the previous runs. This condition is always true when the node is started for the
very first time.

## `--etl.buffersize`

This argument is an option. It specifies the maximum amount of ETL (Extract Transform Load) data that can be stored in
memory before being flushed to disk.
Each time this threshold is reached, the ETL data is flushed to a temporary file in the etl-tmp directory. During the
load phase of ETL the data is retrieved from the temporary files and stored into the db. At end of each batch process
the temporary etl files are deleted.

The default value is 256MiB. If you want to change this value you can pass the argument such as:

```bash
$ ./zenpp [...] --etl.buffersize=128MiB
```

where `128MiB` is the new maximum amount of ETL data you're willing to allow in memory.

**Note. Adopting too small values might have an impact on overall performances as the process requires more IO
operations**

## Syncloop

In this section we analyze all the arguments related to the [syncronization loop](concepts-staged-sync.md).

### `--syncloop.batchsize`

This argument is an option. It specifies, for each stage, the maximum amount of bytes to process before committing to
the database.
The default value is 512MiB. If you want to change this value you can pass the argument such as:

```bash
$ ./zenpp [...] --syncloop.batchsize=256MiB
```

where `256MiB` is the new maximum amount of bytes you're willing to process before committing to the database.

**Note. Adopting too small values might have an impact on overall performances as database commits and fsyncs are more
frequent**

### `--syncloop.throttle`

This argument is an option. Sets the minimum delay, in seconds, between sync loop starts. The default value is 0.
It might be useful in conditions where the block get_interval is very narrow and the node keeps staying in sync with the tip
of the chain while you don't have interest in such a realt time update: maybe you use node data for statistical purposes
and you don't want to waste resources in keeping the node in sync with the tip of the chain.
Generally it does not make sense to have this value set to anything below or equal the average block get_interval of the
chain you're connecting to.
If you want to change this value you can pass the argument such as:

```bash
$ ./zenpp [...] --syncloop.throttle=10
```

where `10` is the new minimum delay, in seconds, between sync loop starts.

### `--syncloop.loginterval`

This argument is an option. Sets the minimum get_interval, in seconds, between printouts of loglines informing about status
of current stage.
The default value is 30. Reducing this value might result in very verbose logs.
If you want to change this value you can pass the argument such as:

```bash
$ ./zenpp [...] --syncloop.loginterval=60
```

## `--fakepow`

This argument is a flag. When set the node will not perform any Proof of Work check on imported blocks. This is useful
when you want to run a node on a system with limited resources and you don't want to waste CPU cycles in performing PoW
checks.
Setting this on mainnet is not generally a good idea as you might end up importing invalid blocks.
On tesnets instead can help speed up the sync process.

## `--zk.nochecksums`

This argument is a flag.
As `zenpp` requires the presence of some zkSNARKs parameters files to be able to verify certain transaction types, it is
essential that those files are not corrupted.
Due to this reason, `zenpp` checks the checksum of the parameters files before loading them and if the test fails it
attempts to download them again from a trusted location.
The verification of the checksum requires a few seconds (~10) to complete and it is performed every time the node
starts.
If you are sure that the parameters files you have are not corrupted you can skip the checksum verification by setting
this flag as:

```bash
$ ./zenpp [...] --zk.nochecksums
```

**Note. If the files are missing, or they don't match the expected size, they get downloaded anyway and the checksums
verifications is mandatory applied after each download.**
