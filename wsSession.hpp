 
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
using tcp = boost::asio::ip::tcp;  

enum SOCKET_STATE{
      CONNECTING,
      CONNECTED,
      DISCONNECTED,
      DISCONNECTING
};

class session : public std::enable_shared_from_this<session>
{
    
    tcp::resolver resolver_;
    websocket::stream<beast::tcp_stream> ws_;

    beast::flat_buffer buffer_RX;
    beast::flat_buffer buffer_TX;

    std::string host_;
    std::string port_;

    SOCKET_STATE state;

    std::queue<boost::asio::mutable_buffer> RX_queue;
    std::queue<boost::asio::mutable_buffer> TX_queue;

    public:
    session             (net::io_context& ioc);
    
    void run            (char const* host, char const* port, char const* text);

    //Attempt to connect to Web Socket Server
    void connect        (char const* host, char const* port);

    //Handlers for web socket async 
    void on_resolve     (beast::error_code ec, tcp::resolver::results_type results);
    void on_connect     (beast::error_code ec, tcp::resolver::results_type::endpoint_type);
    void on_handshake   (beast::error_code ec);
    void on_write       (beast::error_code ec, std::size_t bytes_transferred);
    void on_read        (beast::error_code ec,std::size_t bytes_transferred);
    void on_close       (beast::error_code ec);
    void send_message   (std::string &msg);
    
    std::string         get_last_message();
    SOCKET_STATE        get_socket_state();
};


