#include <boost/asio.hpp>
#include <mutex>
#include <thread>
#include <iostream>
#include <memory>

using namespace boost;

// Function pointer type that points to the callback
// function which is called when a request is complete.
using Callback = void (*)(unsigned int request_id, const std::string &response, const system::error_code &ec);

struct Session
{
    Session(asio::io_service &ios, const std::string &raw_ip_address, unsigned short port_num,
            const std::string &request, unsigned int id, Callback callback) : m_sock_(ios), m_ep_(asio::ip::address::from_string(raw_ip_address), port_num), m_request_(request),
                                                                              m_id_(id), m_callback_(callback), m_was_cancelled_(false) {}

    asio::ip::tcp::socket m_sock_; // TCP Socket for communication
    asio::ip::tcp::endpoint m_ep_; // Remote endpoint
    const std::string m_request_;

    asio::streambuf m_response_buf_; // Streambuf where response will be stored
    std::string m_response_;         // Response as a string

    system::error_code m_ec_; // Error if any occurs during lifecycle

    unsigned int m_id_;   // Unique ID assigned to request
    Callback m_callback_; // Pointer to function to be called when request completes

    bool m_was_cancelled_;
    std::mutex m_cancel_guard_;
};

class AsyncTCPClient
{
public:
    AsyncTCPClient() : m_work_(std::make_unique<asio::io_service::work>(m_ios_)),
                       m_thread_(std::make_unique<std::thread>([this]()
                                                               { m_ios_.run(); }))
    {
    }

    AsyncTCPClient(const AsyncTCPClient &) = delete;
    AsyncTCPClient &operator=(const AsyncTCPClient &) = delete;

    void emulateLongComputationOp(unsigned int duration_sec, const std::string &raw_ip_address,
                                  unsigned short port_num, Callback callback, unsigned int request_id)
    {
        std::string request = "EMULATE_LONG_COMP_OP " + std::to_string(duration_sec) + "\n";

        std::shared_ptr<Session> session = std::make_shared<Session>(m_ios_, raw_ip_address, port_num, request, request_id, callback);

        session->m_sock_.open(session->m_ep_.protocol());

        // Active sessions vector can be accessed from multiple threads
        // Guard it with mutex to prevent data corruption
        std::unique_lock<std::mutex> lock(m_active_sessions_guard_);
        m_active_sessions_[request_id] = session;
        lock.unlock();

        // THIS IS VERY IMPORTANT CODE -> Chaining Asynchronous operations
        session->m_sock_.async_connect(session->m_ep_, [this, session](const system::error_code &ec)
                                       {
            std::cout << session.use_count() << "\n";
            if (ec.value() != 0)
            {
                session->m_ec_ = ec;
                onRequestComplete(session);
                return;
            }                                      
            std::unique_lock<std::mutex> cancel_lock(session->m_cancel_guard_);
            if (session->m_was_cancelled_)
            {
                onRequestComplete(session);
                return;
            }                                           

            asio::async_write(session->m_sock_, asio::buffer(session->m_request_), [this, session](const system::error_code &ec, std::size_t bytes_transferred)
            {
                std::cout << session.use_count() << "\n";
                if(ec.value() != 0)
                {
                    session->m_ec_ = ec;
                    onRequestComplete(session);
                    return;
                }
                std::unique_lock<std::mutex> cancel_lock(session->m_cancel_guard_);
                if(session->m_was_cancelled_)
                {
                    onRequestComplete(session);
                    return;
                }
                asio::async_read_until(session->m_sock_, session->m_response_buf_, '\n', [this,session](const boost::system::error_code& ec, std::size_t bytes_transferred) 
                {
                    std::cout << session.use_count() << "\n";
                    if(ec.value() != 0)
                    {
                        session->m_ec_ = ec;
                    }
                    else
                    {
                        std::istream strm(&session->m_response_buf_);
                        std::getline(strm, session->m_response_);
                    }

                onRequestComplete(session);
                }); 
              
            }); });
    }

    // Cancel the Request
    void cancelRequest(unsigned int request_id)
    {
        std::unique_lock<std::mutex> lock(m_active_sessions_guard_);

        auto it = m_active_sessions_.find(request_id);
        if (it != m_active_sessions_.end())
        {
            std::unique_lock<std::mutex> lock(it->second->m_cancel_guard_);
            it->second->m_was_cancelled_ = true;
            it->second->m_sock_.cancel();
        }
    }
    void close()
    {
        m_work_.reset(nullptr);
        // Wait for I/O thread to join
        m_thread_->join();
    }

private:
    void onRequestComplete(std::shared_ptr<Session> session)
    {
        std::cout << session.use_count() << "\n";
        // Shutting down the connection. Might fail if socket is not connected
        // We don't care if this function fails
        system::error_code ignored_ec;
        session->m_sock_.shutdown(asio::socket_base::shutdown_both, ignored_ec);
        std::unique_lock<std::mutex> lock(m_active_sessions_guard_);
        auto it = m_active_sessions_.find(session->m_id_);
        if (it != m_active_sessions_.end())
        {
            m_active_sessions_.erase(session->m_id_);
        }
        lock.unlock();

        system::error_code ec;
        if (session->m_ec_.value() == 0 && session->m_was_cancelled_)
        {
            ec = asio::error::operation_aborted;
        }
        else
        {
            ec = session->m_ec_;
        }
        // Call the callback provided by the user
        session->m_callback_(session->m_id_, session->m_response_, ec);
    }

private:
    asio::io_service m_ios_;
    std::map<int, std::shared_ptr<Session>> m_active_sessions_;
    std::mutex m_active_sessions_guard_;
    std::unique_ptr<asio::io_service::work> m_work_;
    std::unique_ptr<std::thread> m_thread_;
};

void handler(unsigned int request_id, const std::string &response, const system::error_code &ec)
{
    if (ec.value() == 0)
    {
        std::cout << "Request #" << request_id
                  << " has completed. Response: "
                  << response << std::endl;
    }
    else if (ec.value() == asio::error::operation_aborted)
    {
        std::cout << "Request #" << request_id
                  << " has been cancelled by the user."
                  << std::endl;
    }
    else
    {
        std::cout << "Request #" << request_id
                  << " failed! Error code = " << ec.value()
                  << ". Error message = " << ec.message()
                  << std::endl;
    }
}

int main()
{
    try
    {
        AsyncTCPClient client;
        using namespace std::chrono_literals;
        // Initiate a request with id 1
        client.emulateLongComputationOp(10, "127.0.0.1", 3333,
                                        handler, 1);
        // Then do nothing for 5 seconds
        std::this_thread::sleep_for(5s);

        // Then initiates another request with id 2.
        client.emulateLongComputationOp(11, "127.0.0.1", 3334,
                                        handler, 2);

        // Then cancel request id 1
        client.cancelRequest(1);

        // Then do nothing for 6 seconds
        std::this_thread::sleep_for(6s);

        // Initiate another request
        client.emulateLongComputationOp(12, "127.0.0.1", 3335,
                                        handler, 3);

        // Do nothing for the next 15 seconds
        std::this_thread::sleep_for(15s);

        // Exit the application
        client.close();
    }
    catch (system::error_code &e)
    {
        std::cout << "Error occured! Error code = " << e.value()
                  << ". Message: " << e.message();

        return e.value();
    }
    return 0;
}