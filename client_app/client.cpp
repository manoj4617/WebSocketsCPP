#include <boost/asio/ip/network_v4.hpp>
#include <cstdint>
#include <functional>
#include <iostream>
#include <ostream>
#include <sstream>
#include <websocketpp/client.hpp>
#include <websocketpp/close.hpp>
#include <websocketpp/common/connection_hdl.hpp>
#include <websocketpp/common/memory.hpp>
#include <websocketpp/common/system_error.hpp>
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/logger/levels.hpp>

using namespace std;

typedef websocketpp::client<websocketpp::config::asio_client> client;

class connection_metadata {

    public:
    typedef websocketpp::lib::shared_ptr<connection_metadata> ptr;

    connection_metadata(uint32_t id, websocketpp::connection_hdl hdl, string uri)
    :m_id(id), m_hdl(hdl), m_uri(uri), m_status("Connecting"),
        m_server("N/A")
    {}

    void on_open(client* c, websocketpp::connection_hdl hdl){
        m_status = "Open";
        client::connection_ptr conn = c->get_con_from_hdl(hdl);
        m_server = conn->get_response_header("Server");
        // cout << "> Connection " << m_id << " opened successfully." << endl;
    }

    void on_fail(client* c, websocketpp::connection_hdl hdl){
        m_status = "Failed";
        client::connection_ptr conn = c->get_con_from_hdl(hdl);
        m_server = conn->get_response_header("Server");
        m_error_reason = conn->get_ec().message();

        // cout << "> Connection " << m_id << " failed: " << m_error_reason << endl;
    }
    
    void on_close(client* c, websocketpp::connection_hdl hdl){
        m_status = "Closed";
        client::connection_ptr conn = c->get_con_from_hdl(hdl);
        stringstream ss;
        ss << "close code: " << conn->get_remote_close_code() << " ("
            << websocketpp::close::status::get_string(conn->get_remote_close_code())
            << "), close, reason: " << conn->get_remote_close_reason();
        m_error_reason = ss.str();
    }

    void on_message(websocketpp::connection_hdl hdl, client::message_ptr msg) {
        if (msg->get_opcode() == websocketpp::frame::opcode::text) {
            m_messages.push_back(msg->get_payload());
        } else {
            m_messages.push_back(websocketpp::utility::to_hex(msg->get_payload()));
        }
    }
    void record_sent_message(std::string message) {
       m_messages.push_back(">> " + message);
    }

    websocketpp::connection_hdl get_hdl(){
        return m_hdl;
    }

    uint32_t get_id(){
        return m_id;
    }

    string get_status(){
        return m_status;
    }
    friend ostream& operator<< (ostream& out, connection_metadata const& data);
    private:
    uint32_t m_id;
    websocketpp::connection_hdl m_hdl;
    std::vector<std::string> m_messages;
    string m_status;
    string m_uri;
    string m_server;
    string m_error_reason;
};

std::ostream & operator<< (std::ostream & out, connection_metadata const & data) {
    out << "> URI: " << data.m_uri << "\n"
        << "> Status: " << data.m_status << "\n"
        << "> Remote Server: " << (data.m_server.empty() ? "None Specified" : data.m_server) << "\n"
        << "> Error/close reason: " << (data.m_error_reason.empty() ? "N/A" : data.m_error_reason)
        << "> Messages Processed: (" << data.m_messages.size() << ") \n";
        std::vector<std::string>::const_iterator it;
        for (it = data.m_messages.begin(); it != data.m_messages.end(); ++it) {
            out << *it << "\n";
        }
 
    return out;
}

class websocket_endpoint {

    public:
    websocket_endpoint() : m_next_id(0){
        // clears all access channels for logging the activities of the WebSocket client 
        // (all logs that would normally be printed for incoming WebSocket traffic)
        // websocketpp::log::alevel::all is a log level constant that indicates all 
        // types of access-related logging should be turned off.
        m_endPoint.clear_access_channels(websocketpp::log::alevel::all);

        // clears all error channels for logging error messages that would typically 
        // be printed when there is an issue with the WebSocket connection.
        // websocketpp::log::elevel::all disables all error logging.
        m_endPoint.clear_error_channels(websocketpp::log::elevel::all);

        // Initializes the WebSocket client using ASIO
        m_endPoint.init_asio();

        /*
         *Starts the perpetual operation of the WebSocket client. 
         *This keeps the client running in the background, processing events, and managing the WebSocket connection. 
         *Without this, the client would immediately stop after initialization.
        */
        m_endPoint.start_perpetual();
        
        // Creates a new thread to run client::run method in async
        // client::run starts websocket client loop
        m_thread.reset(new websocketpp::lib::thread(&client::run, &m_endPoint));
    }

    uint32_t connect(const string& uri){
        websocketpp::lib::error_code ec;
        client::connection_ptr conn = m_endPoint.get_connection(uri, ec);
        if(ec){
            cout << "> Connect initialization error: " << ec.message() << endl;
            return -1;
        }
        uint32_t new_id = m_next_id++;
        connection_metadata::ptr metadata_ptr(new connection_metadata(new_id,conn->get_handle(), uri));

        m_connection_list[new_id] = metadata_ptr;

        conn->set_open_handler(websocketpp::lib::bind(
            &connection_metadata::on_open,
            metadata_ptr,
            &m_endPoint,
            websocketpp::lib::placeholders::_1
        ));

        conn->set_fail_handler(websocketpp::lib::bind(
            &connection_metadata::on_fail,
            metadata_ptr,
            &m_endPoint,
            websocketpp::lib::placeholders::_1
        ));

        conn->set_message_handler(websocketpp::lib::bind(
            &connection_metadata::on_message,
            metadata_ptr,
            websocketpp::lib::placeholders::_1,
            websocketpp::lib::placeholders::_2
        ));
        m_endPoint.connect(conn);

        return new_id;
    }

    void close(uint32_t id, websocketpp::close::status::value code, const string& reason){
        websocketpp::lib::error_code ec;

        conn_list::iterator metadata_itr = m_connection_list.find(id);
        if(metadata_itr == m_connection_list.end()){
            cout << "> No Connection found with id: " << id << endl;
            return;
        }

        m_endPoint.close(metadata_itr->second->get_hdl(), code, reason, ec);
        if(ec){
            cout << "> Error initiating close: " << ec.message() << endl;
        }
    }
    connection_metadata::ptr get_metadata(uint32_t id) const{
        conn_list::const_iterator it = m_connection_list.find(id);
        if(it == m_connection_list.end()){
            return connection_metadata::ptr();
        }
        else{
            return it->second;
        }
    }

    void send(uint32_t id, string message){
        websocketpp::lib::error_code ec;
        conn_list::iterator itr = m_connection_list.find(id);
        if(itr == m_connection_list.end()){
            cout << "> No connection found with id " << id << endl;
            return;
        }
        m_endPoint.send(itr->second->get_hdl(), message, websocketpp::frame::opcode::text, ec);
        if (ec) {
            std::cout << "> Error sending message: " << ec.message() << std::endl;
        }
        itr->second->record_sent_message(message);
    }
    
    ~websocket_endpoint(){
        m_endPoint.stop_perpetual();
        for(conn_list::const_iterator it = m_connection_list.begin(); it != m_connection_list.end(); ++it){
            if(it->second->get_status() != "Open"){
                continue;
            }
            cout << "> Closing connection " << it->second->get_id() << endl;

            websocketpp::lib::error_code ec;
            m_endPoint.close(it->second->get_hdl(), websocketpp::close::status::going_away, "", ec);
            if(ec){
                std::cout << "> Error closing connection " << it->second->get_id() << ": "  
                      << ec.message() << std::endl;
            }
        }
        m_thread->join();
    }

    private:
    typedef map<uint32_t, connection_metadata::ptr> conn_list;
    client m_endPoint;
    websocketpp::lib::shared_ptr<websocketpp::lib::thread> m_thread;
    conn_list m_connection_list;
    uint32_t m_next_id;
};

bool processInput(){
    string input;
    websocket_endpoint endpoint;

    cout << "Enter Command: ";
    getline(cin, input);
    
    if(input == "quit" || input == "-q"){
        return true;
    }
    else if(input == "help" || input == "-h"){
        cout << "\nCommand List: \n"
                << "connect <ws uri>\n"
                << "show <connection id>\n"
                << "close <connection id reason>\n"
                << "send <connection id message\n"
                << "help: Display help text\n"
                << "quit: Exit the program\n"
                << endl;
    }
    else if(input.substr(0,7) == "connect"){
        uint32_t id = endpoint.connect(input.substr(8));
        if(id != -1){
            cout << "> Created connection with id: " << id << endl;
        }
    }
    else if(input.substr(0,4) == "show"){
        uint32_t id = atoi(input.substr(5).c_str());
        connection_metadata::ptr metadata = endpoint.get_metadata(id);
        if(metadata){
            cout << *metadata << endl;
        }
        else{
            cout << "> Unrecognized connection id: " << id << endl;
         }
    }
    else if(input.substr(0,5) == "close"){
        stringstream ss(input);
        string cmd;
        uint32_t id;
        int close_code = websocketpp::close::status::normal;
        string reason;
        ss << cmd << id << close_code;
        getline(ss, reason);
        endpoint.close(id, close_code, reason);
    } 
    else if (input.substr(0,4) == "send") {
       std::stringstream ss(input);
        
        std::string cmd;
        int id;
        std::string message = "";
        
        ss >> cmd >> id;
        std::getline(ss,message);
        
        endpoint.send(id, message);
    }
    else{
        cout << "Unrecognised Command\n" << endl;
    }
    return false;
}

int main(){
    while(!processInput()){
        
    }

    return 0;
}