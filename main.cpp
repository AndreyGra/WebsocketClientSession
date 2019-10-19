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
#include <queue>     
#include "wsSession.hpp"


namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

//------------------------------------------------------------------------------

// Report a failure
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

    std::cout << "objects created, running!" << std::endl;


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
