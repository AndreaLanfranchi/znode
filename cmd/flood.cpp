//
// Created by Andrea on 15/06/2023.
//

#include <iostream>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>


using boost::asio::ip::tcp;

int main() {
    boost::asio::io_context io_context;
    boost::asio::ssl::context ssl_context(boost::asio::ssl::context::tlsv12_client);

    std::string nodeIP = "95.216.230.111"; // Replace with the IP address of the target peer
    std::string nodePort = "9033"; // Replace with the port number of the target peer

    try {
        ssl_context.set_default_verify_paths();

        tcp::resolver resolver(io_context);
        tcp::resolver::results_type endpoints = resolver.resolve(tcp::v4(), nodeIP, nodePort);

        boost::asio::ssl::stream<tcp::socket> socket(io_context, ssl_context);
        boost::asio::connect(socket.lowest_layer(), endpoints);
        socket.handshake(boost::asio::ssl::stream_base::client);

        std::cout << "Connected to peer: " << nodeIP << ":" << nodePort << std::endl;

        // Send a "version" message to the peer otherwise we get penalized
        // for every message we send

        // Begin to hammer the peer with unknown messages


        std::this_thread::sleep_for(std::chrono::seconds(5));

        boost::system::error_code ec;
        socket.shutdown(ec);
        socket.lowest_layer().close(ec);

        if (ec) {
            std::cerr << "Error during disconnection: " << ec.message() << std::endl;
        } else {
            std::cout << "Disconnected from peer." << std::endl;
        }

//        // Send a "version" message to the peer
//        std::string versionMessage = "version message payload"; // Replace with your version message payload
//
//        boost::system::error_code error;
//        boost::asio::write(socket, boost::asio::buffer(versionMessage), error);
//        if (error) {
//            throw boost::system::system_error(error);
//        }
//
//
//
//        // Read data from the peer
//        boost::asio::streambuf receiveBuffer;
//        boost::asio::read(socket, receiveBuffer);
//
//        std::istream responseStream(&receiveBuffer);
//        std::string responseData;
//        std::getline(responseStream, responseData);
//
//        std::cout << "Received data: " << responseData << std::endl;
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0;
}
