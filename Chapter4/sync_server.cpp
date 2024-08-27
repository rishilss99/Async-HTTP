#include <boost/asio.hpp>

#include <thread>
#include <atomic>
#include <memory>
#include <iostream>

using namespace boost;

class Service
{
public:
    Service() {}

    void HandleClient(asio::ip::tcp::socket &sock)
    {
        try
        {
            asio::streambuf request;
            asio::read_until(sock, request, "\n");

            // Emulate Request Processing
            for (int i = 0; i < 10e5; i++)
                ;
            std::this_thread::sleep_for(std::chrono::milliseconds(500));

            std::string response = "Response\n";
            asio::write(sock, asio::buffer(response));
        }
        catch (const system::system_error &e)
        {
            std::cout << "Error occured! Error code = " << e.code() << ". Message: " << e.what();
        }
    }
};

class Acceptor
{
public:
    Acceptor(asio::io_service &ios, unsigned short port_num) : m_ios_(ios),
                                                               m_acceptor_(ios, asio::ip::tcp::endpoint(asio::ip::address_v4::any(), port_num))
    {
        m_acceptor_.listen();
    }

    void Accept()
    {
        // Creating an active socket
        asio::ip::tcp::socket sock(m_ios_);

        m_acceptor_.accept(sock);

        Service svc;
        svc.HandleClient(sock);
    }

private:
    asio::io_service &m_ios_;
    asio::ip::tcp::acceptor m_acceptor_;
};

class Server
{
public:
    Server() : m_stop_(false) {}

    void Start(unsigned short port_num)
    {
        m_thread_.reset(new std::thread([this, port_num]()
                                        { Run(port_num); }));
    }

    void Stop()
    {
        m_stop_.store(true);
        m_thread_->join();
    }

private:
    void Run()
    {
    }
    void Run(unsigned short port_num)
    {
        Acceptor ac(m_ios_, port_num);

        while (!m_stop_.load())
        {
            ac.Accept();
        }
    }

    std::atomic<bool> m_stop_;
    asio::io_service m_ios_;
    std::unique_ptr<std::thread> m_thread_;
};

int main()
{
    unsigned short port_num = 3333;
    try
    {
        Server srv;
        srv.Start(port_num);

        std::this_thread::sleep_for(std::chrono::seconds(30));

        srv.Stop();
    }
    catch (const system::system_error &e)
    {
        std::cout << "Error occured! Error code = " << e.code() << ". Message: " << e.what();
    }
    return 0;
}