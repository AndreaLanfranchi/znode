# Table of Contents
- [System Requirements](#system-requirements)
- [Command line arguments](command-line-arguments.md)
- [About Staged Sync](concepts-staged-sync.md)
- [TO DOs](todos.md)
- [Code tree map](#code-tree-map)

## System Requirements
To run an archive node on `zend++` we recommend the following minimum system requirements:
- 4+ CPU cores
- 16+ GiB RAM
- 500+ GiB available storage space (SSD recommended, NVMe optimal)
- 10+ Mbit/s bandwidth internet connection

We strongly discourage running a node on mechanical hard drives (HDD) as it will lead to degraded performance and will struggle to keep up with the tip of the chain.
We also discourage running a node on a VPS __unless__ you have dedicated CPU cores and a dedicated directly attached SSD drive (experiments on AWS with gp2/gp3 storage showed terrible performance).
Do not even try on AWS LightSail instances.

## Code Tree Map
This projects contains the following directory components:
* [`cmake`](../cmake) Where main cmake components are stored. Generally you don't need to edit anything there.
* [`cmd`](../cmd) The basic source code of project's executable binaries (daemon and support tools). Nothing in this directory gets built when you choose the `BUILD_CORE_ONLY` build option
* [`doc`](../doc) The documentation area. No source code is allowed here
* [`third-party`](../third-party) Where most of the dependencies of the project are stored. Some directories may be bound to [submodules] while other may contain imported code.
* [`src/core`](../src/core) This directory contains the heart of the protocol logic and all basic objects and functions. Source code within `core` is suitable for export (as a library) to third-party applications and cannot make use of C++ exceptions (build flags explicitly voids them)
* [`src/app`](../src/app) This directory contains the application implementation : storage access layer, networking layer, and all other features needed for complete functionality of a block-chain node. Sources built from this directory depend on the `core` directory contents.
  
To simplify the building process cmake is configured to make use of GLOB lists of files. As a result a strict naming convention of files (see [Style Guide](../README.md#style-guide)). In addition to that we establish two file names suffix (before extension) reservations:
* `_test` explicitly mark a file to be included in the unit tests target
* `_benchmark` explicitly mark a file to be included in the benchmarks target

