#include <thread>
#include <iostream>
#include <atomic>
#include <boost/asio.hpp>
#include <memory>

using namespace boost;

class Service
{
public:
    Service() = default;

    void startHandlingClient(std::shared_ptr<asio::ip::tcp::socket> sock)
    {
        std::thread t([this, sock]()
                      { handleClient(sock); });
        t.detach();
    }

private:
    void handleClient(std::shared_ptr<asio::ip::tcp::socket> sock)
    {
        try
        {
            asio::streambuf buf;
            asio::read_until(*sock, buf, '\n');

            // Emulate request processing.
            int i = 0;
            while (i != 1000000)
                i++;

            std::this_thread::sleep_for(std::chrono::milliseconds(500));

            std::string message = "Response\n";
            asio::write(*sock, asio::buffer(message));
        }
        catch (const system::system_error &e)
        {
            std::cout << "Error occured! Error code = "
                      << e.code() << ". Message: "
                      << e.what();
        }

        // Perform cleanup (Suicide)
        delete this;
    }
};

class Acceptor
{
public:
    Acceptor(asio::io_service &ios, unsigned short port_num) : m_ios_(ios), m_acceptor_(m_ios_, asio::ip::tcp::endpoint(asio::ip::address_v4::any(), port_num))
    {
        m_acceptor_.listen();
    }

    void accept()
    {
        auto sock_ptr = std::make_shared<asio::ip::tcp::socket>(m_ios_);
        m_acceptor_.accept(*sock_ptr);
        (new Service)->startHandlingClient(sock_ptr);
    }

private:
    asio::io_service &m_ios_;
    asio::ip::tcp::acceptor m_acceptor_;
};

class Server
{
public:
    Server() : m_stop_(false)
    {
    }

    void start(unsigned short port_num)
    {
        m_thread_ = std::make_unique<std::thread>([this, port_num]()
                                                  { run(port_num); });
    }

    void stop()
    {
        m_stop_.store(false);
        m_thread_->join();
    }

private:
    void run(unsigned short port_num)
    {
        Acceptor ac(m_ios_, port_num);
        while (!m_stop_.load())
        {
            ac.accept();
        }
    }
    asio::io_service m_ios_;
    std::unique_ptr<std::thread> m_thread_;
    std::atomic<bool> m_stop_;
};

int main()
{
    unsigned short port_num = 3333;

    try
    {
        Server srv;
        srv.start(port_num);

        std::this_thread::sleep_for(std::chrono::seconds(60));

        srv.stop();
    }
    catch (system::system_error &e)
    {
        std::cout << "Error occured! Error code = "
                  << e.code() << ". Message: "
                  << e.what();
    }

    return 0;
}