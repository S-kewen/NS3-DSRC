#include <stdio.h>
#include "zmq.hpp"
#include <iostream>

using namespace std;

int main(void)
{
    zmq::socket_t zmqRecvSocket, zmqSendSocket;
    zmq::context_t context;
    zmqRecvSocket = zmq::socket_t(context, ZMQ_PULL);
    zmqRecvSocket.connect("tcp://127.0.0.1:5557");
    zmq::message_t message;
    for(;;){
        zmqRecvSocket.recv(message, zmq::recv_flags::none);
        std::string s(static_cast<char*>(message.data()), message.size());
        cout << s << endl;
    }
    return 0;
}