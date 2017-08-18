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

#include <string>
#include <iostream>
#include <iomanip>
#include <ctime>
#include <sstream>
#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <unistd.h>
#include <gflags/gflags.h>
#include <boost/thread.hpp>

#include <zmqpp/zmqpp.hpp>

#include "data-functions.h"

using std::cout;
using std::endl;
using std::string;


// GLOBAL VARIABLES
boost::thread receiverThread;


DEFINE_string(dbIP, "127.0.0.1", "IP address of the database");
DEFINE_string(dbPWD, "password", "password of the database");
DEFINE_string(dbUSER, "username", "database user");
DEFINE_string(dbNAME, "nameles", "database name");
DEFINE_string(notify_sockets, "none", "String with space separated sockets to notify score updates [tcp://ip:port]");
DEFINE_string(python_script_name, "./scripts/nameles-endofday.py", "Relative or absolute path to python script for computing scores at the end of the day");
DEFINE_int32(nWorkers, 4, "Number of workers");
DEFINE_int32(max_msgs, 1e6, "Maximum number of messages per thread per database serialization");
DEFINE_int32(rcvport, 58510, "\"Receive in\" port");

void SIGINT_handler(int s){
           printf("Caught signal %d\n",s);
           receiverThread.interrupt();
}



int main(int argc, char *argv[]) {
	google::ParseCommandLineFlags(&argc, &argv, true);

	string receiveFromSocket = "tcp://*:" + std::to_string(FLAGS_rcvport);
	string dbConnect = "dbname=" + FLAGS_dbNAME + " user="+ FLAGS_dbUSER +
											" host="+ FLAGS_dbIP + " port=5430 password=" + FLAGS_dbPWD;

	MessagesQueue requests_queue;
	std::unique_ptr<boost::thread> notifyThread;
	receiverThread = boost::thread(receive_msgs, &requests_queue, receiveFromSocket);

	if (FLAGS_notify_sockets != "none"){
		notifyThread = std::unique_ptr<boost::thread>(new boost::thread(compute_scores, argv[0], FLAGS_python_script_name, FLAGS_notify_sockets));
	}
	boost::thread_group workers;
	for( int x=0; x<FLAGS_nWorkers; ++x ) {
    workers.add_thread(new boost::thread(queue_consumer, &requests_queue,
			 																		dbConnect, FLAGS_max_msgs));
	}
	struct sigaction sigIntHandler;

	sigIntHandler.sa_handler = SIGINT_handler;
	sigemptyset(&sigIntHandler.sa_mask);
	sigIntHandler.sa_flags = 0;

	sigaction(SIGINT, &sigIntHandler, NULL);
	receiverThread.join();
	workers.interrupt_all();
	if (FLAGS_notify_sockets != "none"){
		notifyThread->interrupt();
		notifyThread->join();
	}
	workers.join_all();
  return 0;
}
