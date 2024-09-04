#include <boost/asio.hpp>
#include <iostream>

using namespace boost;

// auto setupConnection(asio::io_service &ios);

std::string readFromSocketReadSome(asio::ip::tcp::socket &);

std::string readFromSocketRead(asio::ip::tcp::socket &);

void readFromSocketAsyncReadSome(std::shared_ptr<asio::ip::tcp::socket>, std::shared_ptr<char[]> buf);

void communicate(asio::ip::tcp::socket &);

struct SessionAsyncReadSome
{
    std::shared_ptr<asio::ip::tcp::socket> sock;
    std::shared_ptr<char[]> buf;
    std::size_t totalBytesRead;
    std::size_t bufSize;
};

int main()
{
    asio::ip::address ip_address = asio::ip::address_v4::any();
    unsigned short port_num = 3333;
    asio::io_service ios;
    system::error_code ec;
    asio::ip::tcp::endpoint ep(ip_address, port_num);
    const int queue_size = 120;
    try
    {
        asio::ip::tcp::acceptor acceptor_socket{ios, ep.protocol()};
        acceptor_socket.bind(ep);
        acceptor_socket.listen(queue_size);
        auto sock = std::make_shared<asio::ip::tcp::socket>(ios);
        acceptor_socket.accept(*sock);

        // Connection established read message
        std::string readMsg;
        asio::mutable_buffer inputBuf = asio::buffer(readMsg);

        std::string message = readFromSocketReadSome(*sock);

        std::cout << message;

        message = readFromSocketRead(*sock);

        std::cout << message;

        const std::size_t MESSAGE_SIZE = 17;
        auto buf = std::shared_ptr<char[]>(new char[MESSAGE_SIZE]);

        readFromSocketAsyncReadSome(sock, buf);

        ios.run();
        std::cout << buf.get() << "\n";

        communicate(*sock);

        sock->close();
    }
    catch (system::system_error &e)
    {
        std::cout << "Error occured! Error code = " << e.code() << ". Message: " << e.what();
        return e.code().value();
    }

    return 0;
}

std::string readFromSocketReadSome(asio::ip::tcp::socket &sock)
{
    const std::size_t MESSAGE_SIZE = 15;
    char buffer[MESSAGE_SIZE];
    std::size_t totalBytesRead = 0;

    while (totalBytesRead != MESSAGE_SIZE)
    {
        totalBytesRead += sock.read_some(asio::buffer(buffer + totalBytesRead, MESSAGE_SIZE - totalBytesRead));
    }

    return std::string(buffer);
}

std::string readFromSocketRead(asio::ip::tcp::socket &sock)
{
    const std::size_t MESSAGE_SIZE = 24;
    char buf[MESSAGE_SIZE];

    asio::read(sock, asio::buffer(buf, MESSAGE_SIZE));

    return std::string(buf);
}

void callbackAsyncReadSome(const boost::system::error_code &ec,
                           std::size_t bytes_transferred,
                           std::shared_ptr<SessionAsyncReadSome> s)
{
    if (ec.value() != 0)
    {
        std::cout << "Error occured! Error code = "
                  << ec.value()
                  << ". Message: " << ec.message();
        return;
    }

    s->totalBytesRead += bytes_transferred;

    if (s->totalBytesRead == s->bufSize)
    {
        return;
    }

    s->sock->async_read_some(asio::buffer(s->buf.get() + s->totalBytesRead, s->bufSize - s->totalBytesRead), std::bind(callbackAsyncReadSome, std::placeholders::_1, std::placeholders::_2, s));
}

void readFromSocketAsyncReadSome(std::shared_ptr<asio::ip::tcp::socket> sock, std::shared_ptr<char[]> buf)
{

    auto session = std::make_shared<SessionAsyncReadSome>();

    const std::size_t MESSAGE_SIZE = 17;
    session->sock = std::move(sock); // Move assignment operator used
    session->buf = std::move(buf);
    session->totalBytesRead = 0;
    session->bufSize = MESSAGE_SIZE;
    session->sock->async_read_some(asio::buffer(session->buf.get(), session->bufSize), std::bind(callbackAsyncReadSome, std::placeholders::_1, std::placeholders::_2, session));
}

void communicate(asio::ip::tcp::socket &sock)
{
    asio::streambuf request_buf;
    system::error_code ec;
    asio::read(sock, request_buf, ec);

    if (ec.value() == asio::error::eof)
    {
        // Whole response message has been received
        std::istream is(&request_buf);
        std::string msg;
        std::getline(is, msg);
        std::cout << msg;
    }
    else
    {
        throw system::system_error(ec);
    }

    const char response_buf[] = {0x48, 0x69, 0x21};
    asio::write(sock, asio::buffer(response_buf));
    sock.shutdown(asio::socket_base::shutdown_send);
}