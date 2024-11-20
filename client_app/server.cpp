#include <websocketpp/logger/levels.hpp>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

using logLevel = websocketpp::log::alevel;
typedef websocketpp::server<websocketpp::config::asio> server;

class utility_server {
    public:
    utility_server(){
        m_endpoint.set_error_channels(logLevel::all);
        m_endpoint.set_access_channels(logLevel::all ^ logLevel::frame_payload);

        m_endpoint.init_asio();
    }

    void run(){
        m_endpoint.listen(9002);
        m_endpoint.start_accept();
        m_endpoint.run();
    }
    private:
    server m_endpoint;
};

int main(){
    utility_server websocket_server;
    websocket_server.run();

    return 0;
}