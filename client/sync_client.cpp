#include <boost/asio.hpp>
#include <iostream>

using namespace boost;

class SyncTCPClient
{
public:
    SyncTCPClient(const std::string &ip_address, unsigned short port_num) : m_ep_(asio::ip::address::from_string(ip_address), port_num),
                                                                            m_sock_(m_ios_) // This is just creating the socket
    {
        checkEndpointProtocol();
        m_sock_.open(m_ep_.protocol()); // This is opening the socket -> Need to specify IP: v4 or v6
    }

    void connect()
    {
        m_sock_.connect(m_ep_);
    }

    void close()
    {
        m_sock_.shutdown(asio::ip::tcp::socket::shutdown_both);
        m_sock_.close();
    }

    std::string emulateLongComputationOp(unsigned int duration_sec)
    {
        std::string request = "EMULATE_LONG_COMP_OP " + std::to_string(duration_sec) + "\n";
        sendRequest(request);
        return receiveResponse();
    }

private:
    void checkEndpointProtocol()
    {
        if (m_ep_.protocol() == asio::ip::tcp::v4())
        {
            std::cout << "Uses ipv4 underlying protocol\n";
        }
        if (m_ep_.protocol() == asio::ip::tcp::v6())
        {
            std::cout << "Uses ipv6 underlying protocol\n";
        }
    }

    void sendRequest(const std::string &request)
    {
        asio::write(m_sock_, asio::buffer(request));
    }

    std::string receiveResponse()
    {
        asio::streambuf buf;
        asio::read_until(m_sock_, buf, '\n');

        std::istream input(&buf);

        std::string response;
        input >> response;
        return response;
    }

private:
    asio::io_service m_ios_;
    asio::ip::tcp::endpoint m_ep_;
    asio::ip::tcp::socket m_sock_;
};

void SyncTCPMain()
{
    const std::string ip_address = "127.0.0.1";
    unsigned short port_num = 3333;

    try
    {
        SyncTCPClient client(ip_address, port_num);

        client.connect();

        std::cout << "Sending Request to server....\n";

        std::string response = client.emulateLongComputationOp(10);

        std::cout << "Response received: " << response << "\n";

        client.close();
    }
    catch (const system::system_error &e)
    {
        std::cerr << "Error " << e.what() << '\n';
    }
}

class SyncUDPClient
{
public:
    SyncUDPClient() : m_sock_(m_ios_)
    {
        m_sock_.open(asio::ip::udp::v4());
    }

    std::string emulateLongComputationOp(unsigned int duration_sec, const std::string &raw_address, unsigned short port_num)
    {
        std::string request = "EMULATE_LONG_COMP_OP " + std::to_string(duration_sec) + "\n";
        asio::ip::udp::endpoint ep(asio::ip::address::from_string(raw_address), port_num);
        sendRequest(ep, request);
        return receiveResponse(ep);
    }

private:
    void sendRequest(asio::ip::udp::endpoint &ep, const std::string &request)
    {
        m_sock_.send_to(asio::buffer(request), ep);
    }

    std::string receiveResponse(asio::ip::udp::endpoint &ep)
    {
        char response[6];

        std::size_t bytes_received = m_sock_.receive_from(asio::buffer(response), ep);
        m_sock_.shutdown(asio::ip::udp::socket::shutdown_both);
        return std::string(response, bytes_received);
    }

private:
    asio::io_service m_ios_;
    asio::ip::udp::socket m_sock_;
};

void SyncUDPMain()
{
    const std::string server1_ip_address = "127.0.0.1";
    const short server1_port_num = 3333;

    const std::string server2_ip_address = "192.168.1.10";
    const short server2_port_num = 3334;

    try
    {
        SyncUDPClient client;
        std::cout << "Sending request to the server #1 ... " << std::endl;

        std::string response = client.emulateLongComputationOp(10, server1_ip_address, server1_port_num);

        std::cout << "Response from the server #1 received: " << response << std::endl;

        std::cout << "Sending request to the server #2... " << std::endl;

        response = client.emulateLongComputationOp(10, server2_ip_address, server2_port_num);

        std::cout << "Response from the server #2 received: " << response << std::endl;
    }
    catch (const system::system_error &e)
    {
        std::cout << "Error occured! Error code = " << e.code()
                  << ". Message: " << e.what();

    }
}

int main()
{
}