# Command line arguments
- [Documents Index](README.md)
- [Usage](#usage)
- [--help](#--help)
- [--data-dir](#--data-dir)
- [--chaindata.*](#--chaindata)
  - [--chaindata.exclusive](#--chaindata.exclusive)
  - [--chaindata.readahead](#--chaindata.readahead)
  - [--chaindata.maxsize](#--chaindata.maxsize)
  - [--chaindata.growthsize](#--chaindata.growthsize)
- [--etl.buffersize](#--etl.buffersize)
- [--syncloop.](#--syncloop)
  - [--syncloop.batchsize](#--syncloop.batchsize)
  - [--syncloop.throttle](#--syncloop.throttle)
  - [--syncloop.loginterval](#--syncloop.loginterval)
- [--fakepow](#--fakepow)
- [--zk.nochecksums](#--zk.nochecksums)

## Usage
Executing the node instance is, in most of the cases, pretty straight forward.
All command line arguments are already set to default values we have found to be the most efficient.
As a result you can just run the following command to start the node:
```bash
$ ./<build_dir>/zend++
```
where `<build_dir>` is the directory where the node was built. 
If you have downloaded a pre-built binary, you can just run `zend++` from the directory where it was extracted.

_For Windows users the binary is named `zend++.exe`_

However, if you want to change some of the default values, you have always the option to pass them as command line arguments.
To see the full list of available arguments, you can run the following command:
```bash
$ ./zend++ --help
```
An argument name is always preceded by two dashes (`--`) and an explicative name. When argument names contain dots (`.`) they indicate the are related to a scope.
For example all arguments related to the settings for chaindata db parameters are prefixed with `--chaindata.`.
Arguments are divided in two categories:
- **Flags**: arguments which are false by default and only if explicitly set in command line become true. For example `--chaindata.readahead` is a flag.
- **Options**: arguments which have a default value and can be changed by the user. For example `--data-dir` is an option. 

Arguments can be passed in two ways:
- **Long form**: `--<argument-name>=<argument-value>`
- **Short form**: `--<argument-name> <argument-value>`

Here we analyze the usage and meaning of each argument.

## `--help`
This argument prints the list of available arguments and their meaning.
When this argument is detected `zend` will print the list **and exit** regardless any other argument.

## `--data-dir`
This argument specifies the directory where the node will store all the data and/or expects some useful data to be found.
By default the value of datadir is OS dependent and is determined in this way:
- Windows: `%APPDATA%\Roaming\.zen`
- Linux: `$HOME/.local/share/zen`
- Mac: `$HOME/Library/Application Support/zen`
The structure of the zen directory is the following:
```
zen
├── chaindata    // where the blockchain data (in MDBX format) is stored
├── etl-tmp      // where temporary ETL files are stored
├── nodes        // where the nodes information (in MDBX format) is stored
├── ssl-cert     // where the SSL certificate(s) are stored
└── zk-params    // where the zkSNARK parameters are stored
```
Zen directory (and all subdirs) is created automatically if it does not exist.
It is expected that the user has read/write permissions on the zen directory.
Consider also that the chaindata directory can grow up to hundreds GiB of data (on behalf of the network you're connecting to) and, overall, the space required by zend directory might require up to 300GiB:
please ensure that you have enough space on the partition where the zen directory is located (generally the primary one).
If you don't have enough space there, or simply want to have the zend directory positioned elsewhere, you can always change the location of the zen directory by passing the `--data-dir` argument such as:
```bash
$ ./zend++ --data-dir=/path/to/parent/dir
```
where `/path/to/parent/dir` is the path to the directory where you want to store the zen directory.
The argument can also be specified as relative to the directory of the `zend++` binary:
```bash
$ ./zend++ --data-dir=../zen
```
**Warning. zend is a quite intensive IO application and it is recommended to have access to a storage directory on a directly attached support. For decent performance SSD is recommended. NVMe is even better. Avoid usage of network attached supports: IO throughput is not as important as access latency (a lot of random accesses are performed on the database).
If you intend to install and run zend++ on an AWS instance be advised that using gp2/gp3 storage performances might result disappointing**

## `--chaindata.`
In this section we analyze all the arguments related to the chaindata db parameters.

### `--chaindata.exlusive`
This argument is a flag. When set the chaindata db is opened in exclusive mode: this means that only `zend++` process can access the chaindata db.

### `--chaindata.readahead`
This argument is a flag. When set the MDBX engine will try to read and cache more data than actually required.
On some systems this can improve the performance of the node but your mileage may vary.

### `--chaindata.maxsize`
This argument is an option. It specifies the maximum size of the chaindata db in bytes. 
When the size of the database reaches this threshold, the node won't be able to store new data and will stop due to errors.
However you can restart the node setting a larger value if you have space available.
The default value is 1TiB. If you want to change this value you can pass the argument such as:
```bash
$ ./zend++ --chaindata.maxsize=2TiB
```
where `2TiB` is the new maximum size of the chaindata db you're willing to allow.

### `--chaindata.growthsize`
This argument is an option. It specifies the increment size of the chaindata db in bytes.
The default value is 2GiB which also corresponds to the initial size of the database. Wheneveer MDBX engine needs more space it will extend the database file by this amount of bytes. 
If you want to change this value you can pass the argument such as:
```bash
$ ./zend++ --chaindata.growthsize=4GiB
```
**Warning. The extension of the database file is a costly operation: in particular on Windows it requires the whole memory remapping of the database. Don't set this value to small amounts, especially during initial syncs from scratch, to not impair performance on more frequent extensions**

### `--chaindata.pagesize`
This argument is an option. It specifies the page size MDBX has to use.

_MDBX segments its database file in pages where each page has the size here specified.
MDBX's databases can have very large data sizes but are capped to a maximum 2147483648 (0x80000000ULL) hence
the maximum size a database file can reach is a function of pagesize.
The min value is 256B (which translates into a max db file size of 512GiB) while the max value is 65KiB (which translates into a max db file size of 128TiB)_

The default value is 4KiB which gives room up to 8TiB of data **and is also the default page size for memory allocation by modern OSes**.
If you want to change this value you can pass the argument such as:
```bash
$ ./zend++ --chaindata.pagesize=8KiB
```
where `8KiB` is the new page size you're willing to use.

## `--etl.buffersize`
This argument is an option. It specifies the maximum amount of ETL (Extract Transform Load) data that can be stored in memory before being flushed to disk.
Each time this threshold is reached, the ETL data is flushed to a temporary file in the etl-tmp directory. During the load phase of ETL the data is retrieved from the temporary files and stored into the db. At end of each batch process the temporary etl files are deleted.

The default value is 256MiB. If you want to change this value you can pass the argument such as:
```bash
$ ./zend++ --etl.buffersize=128MiB
```
where `128MiB` is the new maximum amount of ETL data you're willing to allow in memory.

**Note. Adopting too small values might have an impact on overall performances as the process requires more IO operations**

## `--syncloop.`
In this section we analyze all the arguments related to the [syncronization loop](staged_sync.md).

### `--syncloop.batchsize`
This argument is an option. It specifies, for each stage, the maximum amount of bytes to process before committing to the database.
The default value is 512MiB. If you want to change this value you can pass the argument such as:
```bash
$ ./zend++ --syncloop.batchsize=256MiB
```
where `256MiB` is the new maximum amount of bytes you're willing to process before committing to the database.

**Note. Adopting too small values might have an impact on overall performances as database commits and fsyncs are more frequent**

### `--syncloop.throttle`
This argument is an option. Sets the minimum delay, in seconds, between sync loop starts. The default value is 0. 
It might be useful in conditions where the block interval is very narrow and the node keeps staying in sync with the tip of the chain while you don't have interest in such a realt time update: maybe you use node data for statistical purposes and you don't want to waste resources in keeping the node in sync with the tip of the chain.
Generally it does not make sense to have this value set to anything below or equal the average block interval of the chain you're connecting to.
If you want to change this value you can pass the argument such as:
```bash
$ ./zend++ --syncloop.throttle=10
```
where `10` is the new minimum delay, in seconds, between sync loop starts.

### `--syncloop.loginterval`
This argument is an option. Sets the minimum interval, in seconds, between printouts of loglines informing about status of current stage.
The default value is 30. Reducing this value might result in very verbose logs. 
If you want to change this value you can pass the argument such as:
```bash
$ ./zend++ --syncloop.loginterval=60
```

## `--fakepow`
This argument is a flag. When set the node will not perform any Proof of Work check on imported blocks. This is useful when you want to run a node on a system with limited resources and you don't want to waste CPU cycles in performing PoW checks.
Setting this on mainnet is not generally a good idea as you might end up importing invalid blocks.
On tesnets instead can help speed up the sync process.

## `--zk.nochecksums`
This argument is a flag.
As `zend++` requires the presence of some zkSNARKs parameters files to be able to verify certain types transactions, it is essential that those files are not corrupted.
Due to this reason, `zend++` checks the checksum of the parameters files before loading them and if the test fails it attempts to download them again from a trusted location.
The verification of the checksum requires a few seconds (~10) to complete and it is performed every time the node starts.
If you are sure that the parameters files you have are not corrupted you can skip the checksum verification by setting this flag as:
```bash
$ ./zend++ --zk.nochecksums
```
**Note. If the files are missing or they don't match the expected size they get downloaded anyway and the checksums verifications is mandatorily applied after each download.**
