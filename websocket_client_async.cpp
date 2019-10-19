//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

//------------------------------------------------------------------------------
//
// Example: WebSocket client, asynchronous
//
//------------------------------------------------------------------------------

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/strand.hpp>
#include <boost/thread.hpp>
#include <boost/chrono.hpp>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <queue>          // std::queue


namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

//------------------------------------------------------------------------------

// Report a failure
void fail(beast::error_code ec, char const* what)
{
    std::cerr << what << ": " << ec.message() << "\n";
}

enum SOCKET_STATE{
    CONNECTING,
    CONNECTED,
    DISCONNECTED,
    DISCONNECTING
};

// Sends a WebSocket message and prints the response
class session : public std::enable_shared_from_this<session>
{
    tcp::resolver resolver_;
    websocket::stream<beast::tcp_stream> ws_;
    beast::flat_buffer buffer_RX;
    beast::flat_buffer buffer_TX;
    std::string host_;
    std::string port_;
    std::string text_;
    SOCKET_STATE state = DISCONNECTED;
    std::queue<boost::asio::mutable_buffer> RX_queue;
    std::queue<boost::asio::mutable_buffer> TX_queue;



public:
    // Resolver and socket require an io_context
    explicit
    session(net::io_context& ioc): resolver_(net::make_strand(ioc)), ws_(net::make_strand(ioc))
    {}

    // Start the asynchronous operation
    void run(char const* host,
             char const* port,
             char const* text)
    {
        // Save these for later
        host_ = host;
        text_ = text;

        // Look up the domain name
        resolver_.async_resolve(host,
                                port,
                                beast::bind_front_handler(
                                    &session::on_resolve,
                                    shared_from_this()));        
    }

    void connect(   char const* host,
                    char const* port
                ) 
    {   
        this->host_ = host;
        this->port_ = port;

        this->state = SOCKET_STATE::CONNECTING;

        // Look up the domain name
        resolver_.async_resolve(host,
                                port,
                                beast::bind_front_handler(
                                    &session::on_resolve,
                                    shared_from_this()));  

    }

    void on_resolve( beast::error_code ec,
                     tcp::resolver::results_type results)
    {
        if(ec) {
            this->state = SOCKET_STATE::DISCONNECTED;
            return fail(ec, "resolve");
        }
            

        // Set the timeout for the operation
        beast::get_lowest_layer(ws_).expires_after(std::chrono::seconds(30));


        // Make the connection on the IP address we get from a lookup
        beast::get_lowest_layer(ws_).async_connect( results,
                                                    beast::bind_front_handler(
                                                    &session::on_connect,
                                                    shared_from_this()
                                                    ));
    }

    void on_connect(beast::error_code ec,
                    tcp::resolver::results_type::endpoint_type)
    {
        if(ec) {
            this->state = SOCKET_STATE::DISCONNECTED;
            return fail(ec, "connect");
        }
            

        // Turn off the timeout on the tcp_stream, because
        // the websocket stream has its own timeout system.
        beast::get_lowest_layer(ws_).expires_never();

        // Set suggested timeout settings for the websocket
        ws_.set_option(
            websocket::stream_base::timeout::suggested(
                beast::role_type::client));

        // Set a decorator to change the User-Agent of the handshake
        ws_.set_option(websocket::stream_base::decorator(
            [](websocket::request_type& req)
            {
                req.set(http::field::user_agent, std::string(BOOST_BEAST_VERSION_STRING) + " websocket-client-async");
            }));
    

        // Perform the websocket handshake
        ws_.async_handshake(host_, "/", beast::bind_front_handler( &session::on_handshake,
                                                                    shared_from_this()
                                                                 ));
    }

    void on_handshake(beast::error_code ec)
    {
        if(ec) {
            this->state = SOCKET_STATE::DISCONNECTED;
            return fail(ec, "handshake");
        }
        
        this->state = SOCKET_STATE::CONNECTED;
        // Send the message
        
          ws_.async_read(
            buffer_RX,
            beast::bind_front_handler(
                &session::on_read,
                shared_from_this()));
    }

    void on_write( beast::error_code ec, std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if(ec){
            return fail(ec, "write");
        }

        // if (!this->TX_queue.empty()) {
        //     ws_.async_read(
        //     buffer_TX,
        //     beast::bind_front_handler(
        //         &session::on_read,
        //         shared_from_this()));
        // }
        
    }

    void on_read(
        beast::error_code ec,
        std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        this->RX_queue.push(this->buffer_RX.data());

        if(ec) {
            if (ec.message() == "End of file"){
                    this->state = SOCKET_STATE::DISCONNECTING;
                    ws_.async_close(websocket::close_code::normal,
                        beast::bind_front_handler(
                            &session::on_close,
                            shared_from_this()));
                }

                return fail(ec, "read");
        }
        // Close the WebSocket connection
        // ws_.async_close(websocket::close_code::normal,
        //     beast::bind_front_handler(
        //         &session::on_close,
        //         shared_from_this()));

        this->RX_queue.push(buffer_RX.data());

        this->buffer_RX.clear();

          ws_.async_read(
            buffer_RX,
            beast::bind_front_handler(
                &session::on_read,
                shared_from_this()));

    }

    void on_close(beast::error_code ec)
    {
        if(ec)
            return fail(ec, "close");

        // If we get here then the connection is closed gracefully

        // The make_printable() function helps print a ConstBufferSequence
        // std::cout << beast::make_printable(buffer_RX.data()) << std::endl; 
    }

    void send_message(std::string &msg){

        ws_.async_write( net::buffer(msg),
                         beast::bind_front_handler( &session::on_write,
                                                    shared_from_this()
                                                  )
                        );
    }

    std::string get_last_message(){

       std::string returnValue = std::string();

         if(!(this->RX_queue.empty())){
                returnValue = beast::buffers_to_string(this->RX_queue.front());
                this->RX_queue.pop();
            }

        return returnValue;
        
    }

    SOCKET_STATE get_socket_state(){
        return this->state;
    }
};
//------------------------------------------------------------------------------

int main(int argc, char** argv)
{
    // Check command line arguments.
    if(argc != 4)
    {
        std::cerr <<
            "Usage: websocket-client-async <host> <port> <text>\n" <<
            "Example:\n" <<
            "    websocket-client-async echo.websocket.org 80 \"Hello, world!\"\n";
        return EXIT_FAILURE;
    }
    auto const host = argv[1];
    auto const port = argv[2];
    auto const text = argv[3];

    // The io_context is required for all I/O
    net::io_context ioc;

    auto sesh =  std::make_shared<session>(ioc);

    // sesh->run(host, port, text);


    while (true){
        
        
        switch(sesh->get_socket_state()){
            case DISCONNECTED: sesh->connect(host,port); break;
            case CONNECTING:    break;
            case DISCONNECTING: break;
            case CONNECTED:     
            auto msg = sesh->get_last_message();
                                if (msg.size() > 0){
                                    std::cout << msg << std::endl;
                                                  
                                    std::string tx_msg = "Hello world!";

                                    sesh->send_message(tx_msg);
                                } 
           
            break;
            }

            ioc.poll();
        }
    

    return EXIT_SUCCESS;
}
