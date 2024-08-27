#include <boost/asio.hpp>

#include <iostream>
#include <thread>
#include <atomic>
#include <memory>

using namespace boost;

class Service
{
public:
    Service(std::shared_ptr<asio::ip::tcp::socket> sock) : m_sock_(sock) {}

    void startHandling()
    {
        asio::async_read_until(*m_sock_.get(), m_request_, "\n", [this](const boost::system::error_code &ec, std::size_t bytes_transferred)
                               { onRequestReceived(ec, bytes_transferred); });
    }

private:
    void onRequestReceived(const boost::system::error_code &ec, std::size_t bytes_transferred)
    {
        if (ec.value() != 0)
        {
            std::cout << "Error occured! Error code = "
                      << ec.value()
                      << ". Message: " << ec.message();

            onFinish();
            return;
        }

        m_response_ = processRequest(m_request_);
        asio::async_write(*m_sock_.get(), asio::buffer(m_response_), [this](const boost::system::error_code &ec, std::size_t bytes_transferred)
                          { onResponseSent(ec, bytes_transferred); });
    }

    void onResponseSent(const boost::system::error_code &ec, std::size_t bytes_transferred)
    {
        if (ec.value() != 0)
        {
            std::cout << "Error occured! Error code = "
                      << ec.value()
                      << ". Message: " << ec.message();
        }

        onFinish();
    }

    std::string processRequest(asio::streambuf &request)
    {
        int i = 0;
        while (i != 1000000)
            i++;

        std::this_thread::sleep_for(
            std::chrono::milliseconds(100));

        std::string response = "Response\n";
        return response;
    }

    void onFinish()
    {
        std::cout << "Called me\n";
        delete this;
    }

    asio::streambuf m_request_;
    std::string m_response_;
    std::shared_ptr<asio::ip::tcp::socket> m_sock_;
};

class Acceptor
{
public:
    Acceptor(asio::io_service &ios, unsigned short port_num) : m_ios_(ios), m_acceptor_(ios, asio::ip::tcp::endpoint(asio::ip::address_v4::any(), port_num)), m_is_stopped_(false)
    {
    }

    void start()
    {
        m_acceptor_.listen();
        initAccept();
    }

    void stop()
    {
        m_is_stopped_.store(true);
    }

private:
    void initAccept()
    {
        auto sock = std::make_shared<asio::ip::tcp::socket>(m_ios_);

        m_acceptor_.async_accept(*sock, [this, sock](const boost::system::error_code &error)
                                 { onAccept(error, sock); });
    }

    void onAccept(const boost::system::error_code &ec, std::shared_ptr<asio::ip::tcp::socket> sock)
    {
        std::cout << "Came here\n";
        if (ec.value() != 0)
        {
            (new Service(sock))->startHandling();
        }
        else
        {
            std::cout << "Error occured! Error code = "
                      << ec.value()
                      << ". Message: " << ec.message();
        }

        // Start next async operation only if
        // acceptor has not yet been stopped
        if (!m_is_stopped_.load())
        {
            initAccept();
        }
        else
        {
            m_acceptor_.close();
        }
    }

    asio::io_service &m_ios_;
    asio::ip::tcp::acceptor m_acceptor_;
    std::atomic<bool> m_is_stopped_;
};

class Server
{
public:
    Server() : m_work_(std::make_unique<asio::io_service::work>(m_ios_)) {}

    void start(unsigned short port_num, unsigned int thread_pool_size)
    {
        assert(thread_pool_size > 0);
        m_acc_.reset(new Acceptor(m_ios_, port_num));
        m_acc_->start();
        for (unsigned int i = 0; i < thread_pool_size; i++)
        {
            auto th = std::make_unique<std::thread>(
                ([this]()
                 { m_ios_.run(); }));
            m_thread_pool_.push_back(std::move(th));
        }
    }

    void stop()
    {
        m_acc_->stop();
        m_ios_.stop();
        for (auto &th : m_thread_pool_)
        {
            th->join();
        }
    }

private:
    asio::io_service m_ios_;
    std::unique_ptr<asio::io_service::work> m_work_;
    std::unique_ptr<Acceptor> m_acc_;
    std::vector<std::unique_ptr<std::thread>> m_thread_pool_;
};

const int unsigned DEFAULT_THREAD_POOL_SIZE = 2;

int main()
{
    unsigned short port_num = 3333;

    try
    {
        Server srv;

        unsigned int thread_pool_size = 2 * std::thread::hardware_concurrency();

        if (thread_pool_size == 0)
        {
            thread_pool_size = DEFAULT_THREAD_POOL_SIZE;
        }

        srv.start(port_num, thread_pool_size);

        std::this_thread::sleep_for(std::chrono::seconds(5));

        srv.stop();
    }
    catch (const system::system_error &e)
    {
        std::cout << "Error occured! Error code = "
                  << e.code() << ". Message: "
                  << e.what();
    }
}