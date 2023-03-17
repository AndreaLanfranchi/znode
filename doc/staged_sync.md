# Staged Sync
- [Documents Index](README.md)
- [What is a Staged Sync](#what-is-a-staged-sync)
- [How Staged Sync Works](#how-staged-sync-works)

## What is a Staged Sync
Staged sync is a new approach of synchronizing node's data up to the tip of the blockchain.
The concept has been firstly introduced by [Alexey Akhunov](https://uk.linkedin.com/in/alexey-akhunov-0915222) (aka Alexey Sharp, aka @realLedgerWatch) during the development
of the Ethereum's client known under the name of [Erigon] (formerly known as Turbo-Geth).
You can read more about the original design [here](https://github.com/ledgerwatch/erigon/blob/devel/eth/stagedsync/README.md).

Traditional implementations derived from Bitcoin's code base (like [Zcash](https://github.com/zcash/zcash) and, of course [Zen](https://github.com/HorizenOfficial/zen)) embrace a linear approach:
basically they replay the whole history of the chain from block 0 (aka genesis) to block _N_ in a ordered linear sequence and for each block they update the state.
This causes a very high pressure on I/O layer as, for each block, the previous state must be retrieved, modified and eventually persisted again to allow the restart of the synchronization process in case it gets interrupted for any reason.
Due to this a lot of unneeded information get persisted in the database even if there is no actually need for that.

An example might help clarify the concept: when the node keeps an index of unspent UTXOs we, more often than not, face the case
when at block _N_ an unspent UTXO is recorded in the index and, due to the fact that UTXO is spent on block _N+1_ the entry is
shortly after removed from the index. During the replay of the whole history of the chain using the ordered linear approach this
condition happens quite often causing, as a result, a huge write amplification on the local database.

Here is where Staged Sync comes in help: working by stages we can isolate each logical operation and process data in batches so
the results of each batch (not of each block) is the only one being persisted. The stage logic also mitigates the continuos interleave
of reads and writes on the local database hence speeding up the process considerably.

While the original design by [Erigon] was meant for Ethereum like model (i.e. Account Model) and for the execution of the EVM,
Bitcoin derived chains still rely on UTXO model and don't have the sophisticated Virtual Machine execution. Nevertheless the model's validity
still stands. The purpose of this work is to adapt the Staged Sync model to Bitcoin's UTXO model embedding the unique features
of ZEN blockchain.

This work deeply relies on the implementation of [Silkworm] ([Erigon]'s side project) and inherits the same ethos of quality and efficiency.

## How Staged Sync Works
As the name suggest it consist in the split of the work to synchronize a node with the tip of the chain into stages.
Unlike traditional implementations where _everything happens_ on each processed block, each stage is isolated from the ancestor
(even if it depends on ancestor's results) and relies on its own set of data. In this way we can significantly reduce the reads/writes
interleave and at the same time we significantly reduce code complexity.
Each stage is executed in order and a stage does not stop until the local HEAD is reached for it: this means that, in an ideal
scenario (no network interruptions, the process isn't restarted, etc.), for the full initial sync each stage will be executed exactly once.
After the last stage is finished, the process re-starts from the beginning, by looking for new headers to download.

At its bare bones the Staged Sync can be explained as follows: let's assume a new node instance has been just boostrapped:
1. The node begins to connect to its peers in the network. It's local HEAD is 0 (genesis)
2. The node acknowledges (from connected peers) which is the highest block header (the tip). Say _Hn_
3. It then begins to request peers all the headers from local HEAD to _Hn_ (like in Bitcoin's _headers-first_).4. 
4. After all header up to _Hn_ have been downloaded the node updates its local HEAD and begins to request peers all the blocks bound to downloaded headers.
5. After all requested blocks have been downloaded the node _executes_ blocks in batches. By _execution_ we mean the application of state changes due to transactions contained in each block
6. After the execution the node builds its internal indexes (if any)
7. The loop of stages is now finished. As the phase from point 1. to 6. requires time is very likely the chain has also advanced hence we go-to point 2. again

When, after several cycles, the node is running one full stage cycle processing 1 block at a time it's eventually in sync.

Should the application be restarted in the middle of stage loop, at next restart the cycle restarts from the beginning.
Let's make an example:
1. Node has downloaded 1M headers
2. Node has downloaded 1M blocks (bodies)
3. Node has executed 500k blocks and gets restarted before getting to 1M blocks

At subsequent restart is very likely the chain has advanced, say to 1.1M blocks, hence the node will download 100K headers, 100K blocks and the execution will restart from 500K up to 1.1M.

[Erigon]: https://github.com/ledgerwatch/erigon
[Silkworm]: https://github.com/torquem-ch/silkworm/
