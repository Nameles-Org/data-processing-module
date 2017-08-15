/*
 * Copyright 2017 Antonio Pastor anpastor{at}it.uc3m.es
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <zmqpp/zmqpp.hpp>
#include <iostream>
#include <unistd.h>

using std::cout;
using std::endl;
using std::string;

int main() {
    //  Prepare our context and socket
    zmqpp::context_t context;
    zmqpp::socket_t socket(context, zmqpp::socket_type::pull);
    try {
        socket.bind("tcp://*:1138");
    } catch (zmqpp::zmq_internal_exception &e){
    	cout << "Exception: " << e.what() << endl;
    	socket.close();
    	context.terminate();
    	return -1;
	}
    zmqpp::message_t msg;
    string msg_str;
    // forever loop
    while (msg_str.compare("kill")!=0) {

        //  Wait for next request from client
        socket.receive(msg);
        msg.get(msg_str, 0);
        cout << "Received from client: " << msg_str << endl;
    }
    socket.close();
    context.terminate();
    return 0;
}
