#include "wsSession.hpp"

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;  

    // Resolver and socket require an io_context
    session::session(net::io_context& ioc): resolver_(net::make_strand(ioc)), ws_(net::make_strand(ioc))
    {}

    // Start the asynchronous operation
    void session::connect(   char const* host,
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

    void session::on_resolve( beast::error_code ec,
                     tcp::resolver::results_type results)
    {
        if(ec) {
            this->state = SOCKET_STATE::DISCONNECTED;
            return;
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

    void session::on_connect(beast::error_code ec,
                    tcp::resolver::results_type::endpoint_type)
    {
        if(ec) {
            this->state = SOCKET_STATE::DISCONNECTED;
            return;
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

    void session::on_handshake(beast::error_code ec)
    {
        if(ec) {
            this->state = SOCKET_STATE::DISCONNECTED;
            return;
        }
        
        this->state = SOCKET_STATE::CONNECTED;
        // Send the message
        
          ws_.async_read(
            buffer_RX,
            beast::bind_front_handler(
                &session::on_read,
                shared_from_this()));
    }

    void session::on_write( beast::error_code ec, std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);
        if(ec){ return; }
        
    }

    void session::on_read(
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

                return;
        }

        this->RX_queue.push(buffer_RX.data());

        this->buffer_RX.clear();

          ws_.async_read(
            buffer_RX,
            beast::bind_front_handler(
                &session::on_read,
                shared_from_this()));

    }

    void session::on_close(beast::error_code ec)
    {
        if(ec) return;
        // If we get here then the connection is closed gracefully;
    }

    void session::send_message(std::string &msg){

        ws_.async_write( net::buffer(msg),
                         beast::bind_front_handler( &session::on_write,
                                                    shared_from_this()
                                                  )
                        );
    }

    std::string session::get_last_message(){

       std::string returnValue = std::string();

        if(!(this->RX_queue.empty())){
            returnValue = beast::buffers_to_string(this->RX_queue.front());
            this->RX_queue.pop();
        }

        return returnValue;
        
    }

    SOCKET_STATE session::get_socket_state(){
        return this->state;
    }


