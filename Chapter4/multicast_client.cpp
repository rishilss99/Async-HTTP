#include <boost/asio.hpp>
#include <iostream>

using namespace boost;

int main()
{
    const std::string multicast_ip_address = "239.255.0.1";
    const short port_num = 3000;
    asio::io_service ios;
    asio::ip::udp::endpoint ep(boost::asio::ip::address::from_string("239.255.0.1"), port_num);

    asio::ip::udp::socket sock(ios, ep.protocol());
    sock.bind(ep);

    sock.set_option(asio::ip::multicast::join_group(boost::asio::ip::address::from_string(multicast_ip_address)));
    std::array<char, 1024> buf;
    boost::asio::ip::udp::endpoint sender_endpoint;
    int len = sock.receive_from(asio::buffer(buf), sender_endpoint);
    std::cout.write(buf.data(), len);
    sock.close();
}