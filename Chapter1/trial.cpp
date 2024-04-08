#include <boost/asio.hpp>
#include <iostream>
#include <string>

using namespace boost;

int main()
{
    // Endpoint
    unsigned short port_number = 4000;
    asio::ip::address ip_address = asio::ip::address_v6::any();
    asio::ip::tcp::endpoint ed{ip_address, port_number};

    // Active socket
    asio::io_service ios;
    asio::ip::tcp protocol = asio::ip::tcp::v4();
    asio::ip::tcp::socket sock(ios);
    boost::system::error_code ec;
    sock.open(protocol, ec);

    // Passive socket
    asio::ip::tcp protocolv6 = asio::ip::tcp::v6();
    asio::ip::tcp::acceptor acceptorv6(ios);
    acceptorv6.open(protocolv6, ec);

    // Resolve DNS
    std::string hostName = "www.google.com";
    std::string portNum = "83";
    /**
     * It knows it's looking for a TCP port since you're using the TCP namespace
     */
    asio::ip::tcp::resolver::query resolver_query(hostName, portNum, asio::ip::tcp::resolver::query::numeric_service);
    asio::ip::tcp::resolver resolver(ios);
    asio::ip::tcp::resolver::iterator it = resolver.resolve(resolver_query, ec);
    asio::ip::tcp::resolver::iterator it_end;

    for (; it != it_end; ++it)
    {
        asio::ip::tcp::endpoint ep = it->endpoint();
    }

    // Bind Socket to Endpoint
    unsigned short port_num = 3333;
    asio::ip::tcp::endpoint ep2{asio::ip::address_v4::any(), port_num};
    asio::ip::tcp::acceptor acceptor(ios, ep2.protocol());
    acceptor.bind(ep2);

    // Connecting a socket
    std::string ipAddr = "127.0.0.1";
    unsigned short portNum2 = 3333;

    try
    {
        asio::ip::tcp::endpoint ep1{asio::ip::address::from_string(ipAddr), portNum2};

        // Active socket is instantiated and opened
        asio::ip::tcp::socket sock{ios, ep1.protocol()};
        sock.connect(ep1);
        /**
         * At this point socket 'sock' is connected to the server application
         */
    }
    catch (system::system_error &e)
    {
        std::cerr << e.what() << '\n';
    }

    // Accepting Connections
    const int QUEUE_SIZE = 120;

    unsigned short portNum3 = 3333;

    asio::ip::tcp::endpoint ep3(asio::ip::address_v4::any(), portNum3);

    try
    {
        asio::ip::tcp::acceptor acceptor(ios, ep3.protocol());
        acceptor.bind(ep3);
        acceptor.listen(QUEUE_SIZE);
        asio::ip::tcp::socket sock2(ios);
        acceptor.accept(sock2);
        /**
         * At this point 'sock2' is connected to client application
        */
    }
    catch (system::system_error &e)
    {
        std::cerr << e.what() << '\n';
    }
}