#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>
#include <iostream>

typedef websocketpp::client<websocketpp::config::asio_client> webSocketClient;

int main(){
    webSocketClient client;

    try{

        client.init_asio();
        client.set_message_handler([](websocketpp::connection_hdl hdl, webSocketClient::message_ptr msg){
            std::cout << "Recieved: " << msg->get_payload() << std::endl;
        });

        client.set_open_handler([&client](websocketpp::connection_hdl hdl){
            std::cout << "Connection opened\n";
            client.send(hdl, "Hello, WebSocket!", websocketpp::frame::opcode::text);
        });

        client.set_close_handler([](websocketpp::connection_hdl hdl){
            std::cout << "Connection closed\n";
        });

        websocketpp::lib::error_code ec;
        auto con = client.get_connection("ws://echo.websocket.org", ec);
        if(ec){
            std::cerr << "Connection error: " << ec.message() << std::endl;
        }
        client.connect(con);

        client.run();
    } catch(const std::exception& ex){
        std::cerr << "Exception: " << ex.what() << std::endl;
    }

    return 0;

}