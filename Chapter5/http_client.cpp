#include <boost/asio.hpp>

#include <thread>
#include <iostream>
#include <memory>
#include <mutex>

using namespace boost;

namespace http_errors
{
    enum http_error_codes
    {
        invalid_response = 1
    };

    class http_errors_category : public boost::system::error_category
    {
    public:
        const char *name() const BOOST_SYSTEM_NOEXCEPT override
        {
            return "http_errors";
        }
        std::string message(int e) const override
        {
            switch (e)
            {
            case invalid_response:
                return "Server response cannot be parsed.";
                break;
            default:
                return "Unknown error.";
            }
        }
    };

    const http_errors_category &get_http_errors_category()
    {
        static http_errors_category cat;
        return cat;
    }

    boost::system::error_code make_error_code(http_error_codes e)
    {
        return boost::system::error_code(static_cast<int>(e), get_http_errors_category());
    }

} // namespace http_errors

namespace boost
{
    namespace system
    {
        template <>
        struct is_error_code_enum<http_errors::http_error_codes>
        {
            BOOST_STATIC_CONSTANT(bool, value = true);
        };
    } // namespace system
} // namespace boost

class HTTPClient;
class HTTPResponse;
class HTTPRequest;

using Callback = void (*)(const HTTPRequest &request, const HTTPResponse &response, const system::error_code &ec);

class HTTPResponse
{
public:
    HTTPResponse() : m_response_stream_(&m_response_buf_) {}

    unsigned int getStatusCode() const
    {
        return m_status_code_;
    }

    const std::string &getStatusMessage() const
    {
        return m_status_message_;
    }

    const std::map<std::string, std::string> &getHeaders() const
    {
        return m_headers_;
    }

    const std::istream &getResponse() const
    {
        return m_response_stream_;
    }

private:
    asio::streambuf &getResponseBuf()
    {
        return m_response_buf_;
    }

    void setStatusCode(unsigned int status_code)
    {
        m_status_code_ = status_code;
    }

    void setStatusMessage(const std::string &status_message)
    {
        m_status_message_ = status_message;
    }

    void addHeader(const std::string &name, const std::string &value)
    {
        m_headers_[name] = value;
    }

private:
    friend class HTTPRequest;
    unsigned int m_status_code_;   // HTTP status code
    std::string m_status_message_; // HTTP status message

    // Response headers
    std::map<std::string, std::string> m_headers_;
    asio::streambuf m_response_buf_;
    std::istream m_response_stream_;
};

class HTTPRequest
{
public:
    void setHost(const std::string &host)
    {
        m_host_ = host;
    }

    void setPort(unsigned int port)
    {
        m_port_ = port;
    }

    void setUri(const std::string &uri)
    {
        m_uri_ = uri;
    }

    void setCallback(Callback callback)
    {
        m_callback_ = callback;
    }

    std::string getHost() const
    {
        return m_host_;
    }

    unsigned int getPort() const
    {
        return m_port_;
    }

    const std::string &getUri() const
    {
        return m_uri_;
    }

    unsigned int getId() const
    {
        return m_id_;
    }

    void execute()
    {
        // Ensure that preconditions hold
        assert(m_port_ > 0);
        assert(m_host_.length() > 0);
        assert(m_uri_.length() > 0);
        assert(m_callback_ != nullptr);

        // Prepare resolver query
        // numeric_service means treat port as a number
        asio::ip::tcp::resolver::query resolver_query(m_host_, std::to_string(m_port_), asio::ip::resolver_base::numeric_service);

        std::unique_lock<std::mutex> cancel_lock(m_cancel_mux_);

        if (m_was_cancelled_)
        {
            cancel_lock.unlock();
            onFinish(system::error_code(asio::error::operation_aborted));
            return;
        }

        // Resolve the host name
        m_resolver_.async_resolve(resolver_query, [this](const system::error_code &ec,
                                                         asio::ip::tcp::resolver::iterator iterator)
                                  { onHostNameResolved(ec, iterator); });
    }

    void cancel()
    {
        std::unique_lock<std::mutex> cancel_lock(m_cancel_mux_);

        m_was_cancelled_ = true;

        m_resolver_.cancel();

        if (m_sock_.is_open())
        {
            m_sock_.cancel();
        }
    }

private:
    HTTPRequest(asio::io_service &ios, unsigned int id) : m_port_(DEFAULT_PORT), m_id_(id), m_callback_(nullptr),
                                                          m_sock_(ios), m_resolver_(ios), m_was_cancelled_(false), m_ios_(ios) {}

    void onHostNameResolved(const system::error_code &ec, asio::ip::tcp::resolver::iterator itr)
    {
        if (ec.value() != 0)
        {
            onFinish(ec);
            return;
        }

        std::unique_lock<std::mutex> cancel_lock(m_cancel_mux_);

        if (m_was_cancelled_)
        {
            cancel_lock.unlock();
            onFinish(system::error_code(asio::error::operation_aborted));
            return;
        }

        asio::async_connect(m_sock_, itr, [this](const system::error_code &ec, asio::ip::tcp::resolver::iterator itr)
                            { onConnectionEstablished(ec, itr); });
    }

    void onConnectionEstablished(const system::error_code &ec, asio::ip::tcp::resolver::iterator itr)
    {
        if (ec.value() != 0)
        {
            onFinish(ec);
            return;
        }

        // Compose the request message
        m_request_buf_ += "GET " + m_uri_ + " HTTP/1.1\r\n";

        // Add mandatory header
        m_request_buf_ += "Host: " + m_host_ + "\r\n";

        m_request_buf_ += "\r\n";

        std::unique_lock<std::mutex> cancel_lock(m_cancel_mux_);

        // Checking for cancellation everytime just before initiating next operation
        if (m_was_cancelled_)
        {
            cancel_lock.unlock();
            onFinish(system::error_code(asio::error::operation_aborted));
            return;
        }

        asio::async_write(m_sock_, asio::buffer(m_request_buf_), [this](const system::error_code &ec, size_t bytes_transferred)
                          { onRequestSent(ec, bytes_transferred); });
    }

    void onRequestSent(const system::error_code &ec, size_t bytes_transferred)
    {
        if (ec.value() != 0)
        {
            onFinish(ec);
            return;
        }

        m_sock_.shutdown(asio::ip::tcp::socket::shutdown_send);

        std::unique_lock<std::mutex> cancel_lock(m_cancel_mux_);

        if (m_was_cancelled_)
        {
            cancel_lock.unlock();
            onFinish(system::error_code(asio::error::operation_aborted));
            return;
        }

        // Read the status line
        asio::async_read_until(m_sock_, m_response_.getResponseBuf(), "\r\n", [this](const system::error_code &ec, size_t bytes_transferred)
                               { onStatusLineReceived(ec, bytes_transferred); });
    }

    void onStatusLineReceived(const system::error_code &ec, size_t bytes_transferred)
    {
        if (ec.value() != 0)
        {
            onFinish(ec);
            return;
        }

        // Parse the status line
        std::string http_version;
        std::string str_status_code;
        std::string status_message;

        std::istream response_stream(&m_response_.getResponseBuf());

        response_stream >> http_version;

        if (http_version != "HTTP/1.1")
        {
            // Response is incorrect;
            onFinish(http_errors::invalid_response);
            return;
        }

        response_stream >> str_status_code;

        // Converst status code to integer
        unsigned int status_code = 200;

        try
        {
            status_code = std::stoul(str_status_code);
        }
        catch (std::logic_error &e)
        {
            onFinish(http_errors::invalid_response);
        }

        std::getline(response_stream, status_message, '\r');
        // Remove symbol '\n' from the buffer -> Important since delimiter is '\r'
        response_stream.get();

        m_response_.setStatusCode(status_code);
        m_response_.setStatusMessage(status_message);

        std::unique_lock<std::mutex> cancel_lock(m_cancel_mux_);

        if (m_was_cancelled_)
        {
            cancel_lock.unlock();
            onFinish(system::error_code(asio::error::operation_aborted));
            return;
        }

        // At this point the status code has been received and parsed
        // Read the response headers now

        asio::async_read_until(m_sock_, m_response_.getResponseBuf(), "\r\n\r\n", [this](const system::error_code &ec, size_t bytes_transferred)
                               { onHeadersReceived(ec, bytes_transferred); });
    }

    void onHeadersReceived(const system::error_code &ec, size_t bytes_transferred)
    {
        if (ec.value() != 0)
        {
            onFinish(ec);
            return;
        }

        // Parse and store the headers
        std::string header, header_name, header_value;
        std::istream response_stream(&m_response_.getResponseBuf());

        while (true)
        {
            std::getline(response_stream, header, '\r');

            // Remove '\n' from the stream
            response_stream.get();

            if (header == "")
            {
                break;
            }

            size_t separator_pos = header.find(':');
            if (separator_pos != std::string::npos)
            {
                header_name = header.substr(0, separator_pos);

                if (separator_pos < header.length() - 1)
                {
                    header_value = header.substr(separator_pos + 1);
                }
                else
                {
                    header_value = "";
                }
                m_response_.addHeader(header_name, header_value);
            }

            std::unique_lock<std::mutex> cancel_lock(m_cancel_mux_);

            if (m_was_cancelled_)
            {
                cancel_lock.unlock();
                onFinish(system::error_code(asio::error::operation_aborted));
                return;
            }

            // Now read the response body
            asio::async_read(m_sock_, m_response_.getResponseBuf(), [this](const system::error_code &ec, size_t bytes_transferred)
                             { onResponseReceived(ec, bytes_transferred); });
        }
    }

    void onResponseReceived(const system::error_code &ec, size_t bytes_transferred)
    {
        if (ec.value() == asio::error::eof)
        {
            onFinish(system::error_code());
        }
        else
        {
            onFinish(ec);
        }
    }

    void onFinish(const system::error_code &ec)
    {
        if (ec.value() != 0)
        {
            std::cout << "Error occured! Error code = "
                      << ec.value()
                      << ". Message: " << ec.message();
        }

        m_callback_(*this, m_response_, ec);
    }

private:
    friend class HTTPClient;
    static const unsigned int DEFAULT_PORT = 80;
    // Request paramters.
    std::string m_host_;
    unsigned int m_port_;
    std::string m_uri_;

    // Request Object unique identifier.
    unsigned int m_id_;

    // Callback to be called when request completes.
    Callback m_callback_;

    // Buffer containing the request line.
    std::string m_request_buf_;

    asio::ip::tcp::socket m_sock_;
    asio::ip::tcp::resolver m_resolver_;

    HTTPResponse m_response_;

    bool m_was_cancelled_;
    std::mutex m_cancel_mux_;

    asio::io_service &m_ios_;
};

class HTTPClient
{
public:
    HTTPClient()
    {
        m_work_ = std::make_unique<asio::io_service::work>(m_ios_);
        m_thread_ = std::make_unique<std::thread>([this]()
                                                  { m_ios_.run(); });
    }

    std::shared_ptr<HTTPRequest> createRequest(unsigned int id)
    {
        return std::shared_ptr<HTTPRequest>(new HTTPRequest(m_ios_, id));
    }

    void close()
    {
        // Can use either to stop
        // m_ios_.stop();
        m_work_.reset(nullptr);

        m_thread_->join();
    }

private:
    asio::io_service m_ios_;
    std::unique_ptr<asio::io_service::work> m_work_ = nullptr;
    std::unique_ptr<std::thread> m_thread_ = nullptr;
};

void handler(const HTTPRequest &request, const HTTPResponse &response, const system::error_code &ec)
{
    if (ec.value() == 0)
    {
        std::cout << "Request #" << request.getId()
                  << " has completed. Response: "
                  << response.getResponse().rdbuf();
    }
    else if (ec == asio::error::operation_aborted)
    {
        std::cout << "Request #" << request.getId()
                  << " has been cancelled by the user."
                  << std::endl;
    }
    else
    {
        std::cout << "Request #" << request.getId()
                  << " failed! Error code = " << ec.value()
                  << ". Error message = " << ec.message()
                  << std::endl;
    }
}

int main()
{
    try
    {
        HTTPClient http_client;

        std::shared_ptr<HTTPRequest> request_one = http_client.createRequest(1);

        request_one->setHost("localhost");
        request_one->setUri("/index.html");
        request_one->setPort(3333);
        request_one->setCallback(handler);

        request_one->execute();

        std::shared_ptr<HTTPRequest> request_two = http_client.createRequest(2);

        request_two->setHost("localhost");
        request_two->setUri("/example.html");
        request_two->setPort(3333);
        request_two->setCallback(handler);

        request_two->execute();

        request_two->cancel();

        // Do nothing for 15 seconds, letting the
        // request complete.
        std::this_thread::sleep_for(std::chrono::seconds(5));

        // Closing the client and exiting the application.
        http_client.close();
    }
    catch (system::system_error &e)
    {
        std::cout << "Error occured! Error code = " << e.code()
                  << ". Message: " << e.what();

        return e.code().value();
    }
}
