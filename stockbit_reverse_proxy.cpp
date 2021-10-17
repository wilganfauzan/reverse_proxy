//
// Simple BOOST Asynchronous ASIO reverse proxy.
//
#define WIN32_LEAN_AND_MEAN

// Windows header declarations.
#include <Windows.h>
#include <tchar.h>
#include <strsafe.h>
#include <Winerror.h>

// C++ header declarations
#include <iostream>
using namespace std;

// BOOST ASIO declarations
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/lexical_cast.hpp>
using namespace boost;
using namespace boost::asio;
using namespace boost::asio::ip;

char szServerBuffer[4096]; // Server buffer will be forwarded to client. 4K.
char szClientBuffer[4096]; // Client buffer will be forwarded to server. 4K.

// Global mutex for cout.
boost::mutex g_Mutex;

// Client write Handler.
void writeclient_handler(const boost::system::error_code& ec, std::size_t bytes_transferred)
{
    g_Mutex.lock();
    cout << "Client Sent: " << bytes_transferred << endl;
    g_Mutex.unlock();
}

// Server write Handler.
void write_handler(const boost::system::error_code& ec, std::size_t bytes_transferred)
{
    g_Mutex.lock();
    cout << "Server Sent: " << bytes_transferred << endl;
    g_Mutex.unlock();
}

// Client read handler. As soon as the client receives data, this function is notified.
// Data is forwarded to the server and initiates for further client read asynchronously.
void readclient_handler(const boost::system::error_code& ec, std::size_t bytes_transferred,
    boost::shared_ptr<tcp::socket>& pClientSocket, boost::shared_ptr<tcp::socket>& pPeerSocket)
{
    if (!ec || bytes_transferred != 0)
    {
        g_Mutex.lock();
        cout << "Client Received: " << bytes_transferred << endl;
        g_Mutex.unlock();
        pPeerSocket->async_write_some(boost::asio::buffer(szClientBuffer, bytes_transferred),
            boost::bind(write_handler, boost::asio::placeholders::error, 
                        boost::asio::placeholders::bytes_transferred));

        pClientSocket->async_read_some(boost::asio::buffer
                       (szClientBuffer, sizeof(szClientBuffer)),
            boost::bind(readclient_handler, boost::asio::placeholders::error, 
                        boost::asio::placeholders::bytes_transferred,
                pClientSocket, pPeerSocket));
    }
    else
    {
        g_Mutex.lock();
        cout << "Client receive error: " << ec << endl;
        g_Mutex.unlock();
    }
}

// Client connect handler. When the client is connected to the 
// external server, this function is triggered.
void connect_handler(const boost::system::error_code& ec, 
                     boost::shared_ptr<tcp::socket>& pClientSocket,
    boost::shared_ptr<tcp::socket>& pPeerSocket)
{
    if (!ec)
    {
        g_Mutex.lock();
        cout << "CLient connected:" << endl;
        g_Mutex.unlock();
        pClientSocket->async_read_some(boost::asio::buffer
                       (szClientBuffer, sizeof(szClientBuffer)),
            boost::bind(readclient_handler, boost::asio::placeholders::error, 
                        boost::asio::placeholders::bytes_transferred,
                pClientSocket, pPeerSocket));
    }
    else
    {
        g_Mutex.lock();
        cout << "Client error: " << ec << endl;
        g_Mutex.unlock();
    }
}

// Server read handler. As soon as the server receives data, this function is notified.
// Data is forwarded to the client and initiates for further server read asynchronously.
void read_handler(const boost::system::error_code& ec, std::size_t bytes_transferred,
    boost::shared_ptr<tcp::socket>& pPeerSocket, boost::shared_ptr<tcp::socket>& pClientSocket)
{
    if (!ec || bytes_transferred != 0)
    {
        g_Mutex.lock();
        cout << "Server Received: " << bytes_transferred << endl;
        g_Mutex.unlock();

        pClientSocket->async_write_some(boost::asio::buffer(szServerBuffer, bytes_transferred),
            boost::bind(writeclient_handler, boost::asio::placeholders::error, 
                        boost::asio::placeholders::bytes_transferred));

        pPeerSocket->async_read_some(boost::asio::buffer
                                    (szServerBuffer, sizeof(szServerBuffer)),
            boost::bind(read_handler, boost::asio::placeholders::error, 
                        boost::asio::placeholders::bytes_transferred,
                        pPeerSocket, pClientSocket));
    }
    else
    {
        g_Mutex.lock();
        cout << "Server receive error: " << ec << endl;
        g_Mutex.unlock();
    }
}

// Server accept handler. When the external client connects the server,
// this function is triggered.
void accept_handler(const boost::system::error_code& ec, 
                    boost::shared_ptr<io_context>& pIOContext,
    boost::shared_ptr<tcp::acceptor>& pAcceptor, 
                      boost::shared_ptr<tcp::socket>& pPeerSocket)
{
    if (!ec)
    {
        g_Mutex.lock();
        cout << "Server accepted:" << endl;
        g_Mutex.unlock();
        boost::shared_ptr<tcp::socket> pNewPeerSocket(new tcp::socket(*pIOContext));
        pAcceptor->async_accept(*pNewPeerSocket,
            boost::bind(accept_handler, boost::asio::placeholders::error, 
                        pIOContext, pAcceptor, pNewPeerSocket));

        // Client
        boost::shared_ptr<tcp::socket> pCientSocket1(new tcp::socket(*pIOContext));
        boost::shared_ptr<tcp::resolver> pClientResolver(new tcp::resolver(*pIOContext));
        boost::shared_ptr<tcp::resolver::query> pClientResolverQuery(
            new tcp::resolver::query("127.0.0.1", boost::lexical_cast<string>(16365)));
        tcp::resolver::iterator itrEndPoint = pClientResolver->resolve(*pClientResolverQuery);
        pCientSocket1->async_connect(*itrEndPoint, 
                       boost::bind(connect_handler, boost::asio::placeholders::error,
            pCientSocket1, pPeerSocket));

        Sleep(2000);

        pPeerSocket->async_read_some(boost::asio::buffer
                                    (szServerBuffer, sizeof(szServerBuffer)),
            boost::bind(read_handler, boost::asio::placeholders::error, 
                        boost::asio::placeholders::bytes_transferred,
                pPeerSocket, pCientSocket1));
    }
    else
    {
        g_Mutex.lock();
        cout << "Server accept error: " << ec << endl;
        g_Mutex.unlock();
    }
}

// Keep the server running.
void ReverProxyThread(boost::shared_ptr<io_context>& pIOContext)
{
    while (1)
    {
        try
        {
            boost::system::error_code ec;
            pIOContext->run(ec);
            break;
        }
        catch (std::runtime_error& ex)
        {

        }
    }
}

void _tmain()
{
    // IOContext
    boost::shared_ptr<io_context> pIOContext1(new io_context());
    boost::shared_ptr<io_context::strand> pStrand(new io_context::strand(*pIOContext1));

    // Worker Thread
    boost::shared_ptr<io_context::work> pWork(new io_context::work(*pIOContext1));
    boost::shared_ptr<boost::thread_group> pTG(new boost::thread_group());
    for (int i = 0; i < 2; i++)
    {
        pTG->create_thread(boost::bind(ReverProxyThread, pIOContext1));
    }

    // Resolver
    boost::shared_ptr<tcp::resolver> pResolver(new tcp::resolver(*pIOContext1));
    boost::shared_ptr<tcp::resolver::query>
        pQuery1(new tcp::resolver::query(tcp::v4(), boost::lexical_cast<string>(5800)));
    tcp::resolver::iterator pIter1 = pResolver->resolve(*pQuery1);
    boost::shared_ptr<tcp::acceptor> pAcceptor(new tcp::acceptor(*pIOContext1));

    // Acceptor
    tcp::endpoint Endpoint = (*pIter1);
    pAcceptor->open(Endpoint.protocol());
    pAcceptor->bind(*pIter1);
    pAcceptor->listen();
    boost::shared_ptr<tcp::socket> pPeerSocket(new tcp::socket(*pIOContext1));
    pAcceptor->async_accept(*pPeerSocket,
        boost::bind(accept_handler, boost::asio::placeholders::error, 
                    pIOContext1, pAcceptor, pPeerSocket));

    cin.get();

    system::error_code ec;
    pPeerSocket->close(ec);
    pPeerSocket->shutdown(tcp::socket::shutdown_both, ec);

    pIOContext1->stop();

    pTG->join_all();
}
//