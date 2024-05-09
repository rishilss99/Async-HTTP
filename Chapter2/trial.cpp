#include <boost/asio.hpp>
#include <string>
#include <iostream>

using namespace boost;

int main()
{
    // const_buffer
    const std::string writeMsg = "Damn!";
    asio::const_buffer outputBuf = asio::buffer(writeMsg);

    // mutable_buffer
    std::string readMsg;
    asio::mutable_buffer inputBuf = asio::buffer(readMsg);

    // dynamic_buffer
    asio::streambuf dynamicBuf;
    std::ostream outputStream(&dynamicBuf);
    outputStream << "Hey! How you doing?";
    std::istream inputStreamv1(&dynamicBuf);
    std::string line;
    std::getline(inputStreamv1, line);
    std::cout << line << "\n";
}