#include <boost/asio.hpp>

using namespace boost;

int main()
{
    const std::string multicast_ip_address = "239.255.0.1";
    const short port_num = 3000;
    asio::io_service ios;
    asio::ip::udp::endpoint ep(asio::ip::address::from_string(multicast_ip_address), port_num);

    asio::ip::udp::socket sock(ios, ep.protocol());
    // sock.bind(asio::ip::udp::endpoint(asio::ip::address_v4::any(), 0));
    const std::string message = "Hey Multicast!";
    sock.send_to(asio::buffer(message), ep);

    sock.close();
}