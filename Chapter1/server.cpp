#include <boost/asio.hpp>
#include <iostream>

using namespace boost;

int main()
{
    asio::ip::address ip_address = asio::ip::address_v4::any();
    unsigned short port_num = 3333;
    asio::io_service ios;
    system::error_code ec;
    asio::ip::tcp::endpoint ep(ip_address, port_num);
    const int queue_size = 120;
    try
    {
        asio::ip::tcp::acceptor acceptor_socket{ios, ep.protocol()};
        acceptor_socket.bind(ep);
        acceptor_socket.listen(queue_size);
        asio::ip::tcp::socket sock(ios);
        acceptor_socket.accept(sock);
    }
    catch (system::system_error &e)
    {
        std::cout << "Error occured! Error code = " << e.code() << ". Message: " << e.what();
        return e.code().value();
    }

    return 0;
}
