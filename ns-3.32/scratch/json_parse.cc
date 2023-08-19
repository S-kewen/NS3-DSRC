#include <stdio.h>
#include "zmq.hpp"
#include <iostream>
#include <json/json.h>

using namespace std;

int main(void)
{
    // zmq::socket_t zmqRecvSocket, zmqSendSocket;
    // zmq::context_t context;
    // zmqRecvSocket = zmq::socket_t(context, ZMQ_PULL);
    // zmqRecvSocket.connect("tcp://127.0.0.1:5557");
    // zmq::message_t message;
    // Json::Reader reader;
    // Json::Value json_object;
    // for(;;){
    //     zmqRecvSocket.recv(message, zmq::recv_flags::none);
    //     std::string json_data (static_cast<char*>(message.data()), message.size());
    //     if (reader.parse(json_data, json_object)){
    //         // std::cout << json_object["id"] << std::endl;
    //         // std::cout << json_object["x"] << std::endl;
    //         // std::cout << json_object["y"] << std::endl;
    //         // std::cout << json_object["z"] << std::endl;
    //         // std::cout << json_object["timestamp"] << std::endl;
    //     }
    //     cout << json_data << endl;
    // }
    return 0;
}