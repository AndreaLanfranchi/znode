# Networking 
- [Documents Index](README.md)

[Boost::Asio]: https://www.boost.org/doc/libs/1_81_0/doc/html/boost_asio.html
[node-hub]: ../src/app/network/node_hub.hpp
[node]: ../src/app/network/node.hpp

The way `zenpp` nodes communicate with each other is based on the Bitcoin protocol. 
The protocol is a set of rules that define how nodes communicate with each other. 
The protocol is based on TCP/IP and uses a peer-to-peer network to broadcast transactions and blocks.
The protocol is also used to synchronize the blockchain between nodes. Unlike the bitcoin major node implementations
`zenpp` makes use of [Boost::Asio] to handle, amongst other components, the networking layer: this allow us to leverage the power of asynchronous I/O increasing
parallelism and performance.

The major components of the networking layer are:
- [node-hub] The node-hub is the main entry point for the networking layer. It is responsible for managing the connections to other nodes and for dispatching messages to the appropriate handlers.
- [node] The node class is the representation of the connection to a remote node. It is responsible for the proper message flow and to read and write from and to the underlying socket. Each node has its own strand of execution: this means each node's conversation is serialized within the scope
of its session while allowing other nodes to run concurrently. The concurrency level is function of the number of cores available on the running host. In any case the performances are unmatched.

## Node-hub
The node-hub is the main network component for `zenpp` instance. It is responsible for the creation of nodes instances - whether inbound or outbound - and for the routing of messages:
- inbound messages received from nodes are routed to the appropriate higher level handlers
- outbound messages, generated within the application (e.g. requests for data) are routed to the appropriate node(s) for delivery
As an additional feature node-hub is also responsible for the collection of nodes addressed as advised by DNS seeds or peers.

## Node

