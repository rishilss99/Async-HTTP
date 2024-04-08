#include <boost/asio.hpp>
#include <string>
#include <iostream>

using namespace boost;

int main()
{
    std::string raw_ip_address = "127.0.0.1";
    unsigned short port_num = 3333;
    asio::io_service ios;

    try
    {
        asio::ip::tcp::endpoint ep(asio::ip::address::from_string(raw_ip_address), port_num);
        asio::ip::tcp::socket sock(ios, ep.protocol());
        sock.connect(ep);
        sock.close();
    }
    catch (system::system_error &e)
    {
        std::cout << "Error occured! Error code = " << e.code() << ". Message: " << e.what();
        return e.code().value();
    }
}