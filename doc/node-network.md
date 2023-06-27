# Networking 
- [Documents Index](README.md)

[Boost::Asio]: https://www.boost.org/doc/libs/1_81_0/doc/html/boost_asio.html
[node-hub]: ../zen/node/network/node_hub.hpp
[node]: ../zen/node/network/node.hpp

The way Horizen nodes communicate with each other is based on the Bitcoin protocol. 
The protocol is a set of rules that define how nodes communicate with each other. 
The protocol is based on TCP/IP and uses a peer-to-peer network to broadcast transactions and blocks.
The protocol is also used to synchronize the blockchain between nodes. Unlike the bitcoin major node implementations
zend++ makes use of [Boost::Asio] to handle the network layer: this allow us to leverage the power of asynchronous I/O increasing
parallelism and performance.

The major components of the networking layer are:
- [node-hub] The node-hub is the main entry point for the networking layer. It is responsible for managing the connections to other nodes and for dispatching messages to the appropriate handlers.
- [node] The node class is the representation of a remote node. It is responsible for managing the connection to a remote node and for dispatching messages to the appropriate handlers.

## Node-hub
The node-hub is the main network component of the zend++ instance. It handles the connections with all nodes whether inbound or outbound.
