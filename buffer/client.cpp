#include <boost/asio.hpp>
#include <string>
#include <iostream>
#include <memory>
#include <type_traits>

using namespace boost;

// auto setupConnection(asio::io_service &ios);

void writeToSocketWriteSome(asio::ip::tcp::socket &);

void writeToSocketWrite(asio::ip::tcp::socket &);

void writeToSocketAsyncWriteSome(std::shared_ptr<asio::ip::tcp::socket>);

void communicate(asio::ip::tcp::socket &);

struct SessionAsyncWriteSome
{
    std::shared_ptr<asio::ip::tcp::socket> sock;
    std::string buffer;
    std::size_t totalBytesWritten;
};

int main()
{
    std::string raw_ip_address = "127.0.0.1";
    unsigned short port_num = 3333;
    asio::io_service ios;

    try
    {
        asio::ip::tcp::endpoint ep(asio::ip::address::from_string(raw_ip_address), port_num);
        auto sock = std::make_shared<asio::ip::tcp::socket>(ios, ep.protocol());

        sock->connect(ep);

        // Connnection established send message
        const std::string message = "Damn! We doing it \n";

        asio::const_buffer output_buf = asio::buffer(message);

        writeToSocketWriteSome(*sock);

        writeToSocketWrite(*sock);

        writeToSocketAsyncWriteSome(sock);

        ios.run();

        communicate(*sock);

        sock->close();
    }
    catch (system::system_error &e)
    {
        std::cout << "Error occured! Error code = " << e.code() << ". Message: " << e.what();
        return e.code().value();
    }
}

void writeToSocketWriteSome(asio::ip::tcp::socket &sock)
{
    using namespace std::string_literals;
    const std::string message = "Damn! We here\n\0"s;
    std::size_t totalBytesWritten = 0;

    while (totalBytesWritten != message.length())
    {
        totalBytesWritten += sock.write_some(asio::buffer(message.c_str() + totalBytesWritten, message.length() - totalBytesWritten));
    }
}

void writeToSocketWrite(asio::ip::tcp::socket &sock)
{
    const std::string message = "Damn! Using asio::write\n";

    asio::write(sock, asio::buffer(message));
}

void callbackAsyncWriteSome(const boost::system::error_code &ec,
                            std::size_t bytes_transferred,
                            std::shared_ptr<SessionAsyncWriteSome> s)
{
    if (ec.value() != 0)
    {
        std::cout << "Error occured! Error code = "
                  << ec.value()
                  << ". Message: " << ec.message();
        return;
    }

    s->totalBytesWritten += bytes_transferred;
    if (s->totalBytesWritten == s->buffer.size())
    {
        return;
    }

    s->sock->async_write_some(asio::buffer(s->buffer.c_str() + s->totalBytesWritten, s->buffer.size() - s->totalBytesWritten), std::bind(callbackAsyncWriteSome, std::placeholders::_1, std::placeholders::_2, s));
}

void writeToSocketAsyncWriteSome(std::shared_ptr<asio::ip::tcp::socket> sock)
{

    auto session = std::make_shared<SessionAsyncWriteSome>();

    session->sock = std::move(sock); // Move assignment operator used
    session->buffer = "Hello from async!";
    session->totalBytesWritten = 0;
    session->sock->async_write_some(asio::buffer(session->buffer), std::bind(callbackAsyncWriteSome, std::placeholders::_1, std::placeholders::_2, session));
}

void communicate(asio::ip::tcp::socket &sock)
{
    const char request_buf[] = {0x48, 0x65, 0x0, 0x6c, 0x6c, 0x6f};
    asio::write(sock, asio::buffer(request_buf));
    sock.shutdown(asio::socket_base::shutdown_send);
    
    asio::streambuf response_buf;
    system::error_code ec;
    asio::read(sock, response_buf, ec);

    if(ec.value() == asio::error::eof)
    {
        // Whole response message has been received
        std::istream is(&response_buf);
        std::string msg;
        getline(is, msg);
        std::cout << msg;

    }
    else
    {
        throw system::system_error(ec);
    }
}