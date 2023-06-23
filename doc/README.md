# Table of Contents
- [System Requirements](#system-requirements)
- [Command line arguments](command-line-arguments.md)
- [About Staged Sync](staged_sync.md)
- [TO DOs](todos.md)

## System Requirements
To run an archive node on `zend++` we recommend the following minimum system requirements:
- 4+ CPU cores
- 16+ GiB RAM
- 500+ GiB available storage space (SSD recommended, NVMe optimal)
- 10+ Mbit/s bandwidth internet connection

We strongly discourage running a node on mechanical hard drives (HDD) as it will lead to degraded performance and will struggle to keep up with the tip of the chain.
We also discourage running a node on a VPS __unless__ you have dedicated CPU cores and a dedicated directly attached SSD drive (experiments on AWS with gp2/gp3 storage showed terrible performance).
Do not even try on AWS LightSail instances.