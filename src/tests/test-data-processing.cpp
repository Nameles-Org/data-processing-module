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

#include <cstdlib>
#include <cstdio>
#include <csignal>
#include <unistd.h>
#include <iostream>     // std::cout
#include <vector>
#include <tuple>        // std::tuple, std::get, std::tie, std::ignore
#include <map>
#include <unordered_map>
#include <gflags/gflags.h>
#include <boost/thread.hpp>
#include <pqxx/pqxx>

#include <fstream>

#include "zmqpp/zmqpp.hpp"

#include "../data-functions.h"

using std::cout;
using std::endl;
using std::string;

#define T_MSG   1 // microseconds
#define ONEEXPNINE 1000000000 //1e9

// TO-DO: define arguments for test running time and throughtput
DEFINE_string(day, "161201", "Day of the database to use for the test queries (in format YYMMDD)");
DEFINE_string(dbIP, "127.0.0.1", "IP address of the database");
DEFINE_int32(dbPORT, 5432, "Port of the database");
DEFINE_string(dbPWD, "", "password of the database");
DEFINE_string(dbUSER, "user", "database user");
DEFINE_string(dbNAME, "nameles", "database name");
//DEFINE_int32(test_duration, 60, "Test duration in seconds");
//DEFINE_int32(MPS, 30000, "Queries per second");
DEFINE_int32(port, 58510, "\"Send to\" port");
DEFINE_int32(nWorkers, 4, "Number of workers");
DEFINE_int32(max_msgs, 1e4, "Maximum number of messages per thread per database serialization");

typedef std::tuple<uint32_t,uint32_t,string> msg_tuple;
typedef std::vector<msg_tuple> msgs_list;

long time_diff(const timespec t0, const timespec t1);

void send_msgs(const string& sendToSocket, const string& dbConnect, const int n_msgs);

boost::thread senderThread, receiverThread;

void SIGINT_handler(int s){
					printf("Caught signal %d\n",s);
					receiverThread.interrupt();
					senderThread.interrupt();
}

int main (int argc, char *argv[]){
	google::ParseCommandLineFlags(&argc, &argv, true);

	int n_msgs = 8*FLAGS_max_msgs; //FLAGS_test_duration * FLAGS_MPS;
	string sendToSocket = "tcp://127.0.0.1:" + std::to_string(FLAGS_port);
	string receiveFromSocket = "tcp://*:" + std::to_string(FLAGS_port);
	string dbConnect = "dbname=" + FLAGS_dbNAME + " user="+ FLAGS_dbUSER +
										" host="+ FLAGS_dbIP + " port=" + std::to_string(FLAGS_dbPORT) + " password=" + FLAGS_dbPWD;

	senderThread = boost::thread(send_msgs, sendToSocket, dbConnect, n_msgs);

	MessagesQueue requests_queue;
	receiverThread = boost::thread(receive_msgs, &requests_queue, receiveFromSocket);

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
	senderThread.join();
	cout << "main all sent" << endl;
	receiverThread.join();
	cout << "main receiver down" << endl;
	workers.interrupt_all();
	workers.join_all();

  auto t = std::time(nullptr);
  auto tm = *std::localtime(&t);
  std::ostringstream date_oss;
  date_oss << std::put_time(&tm, "%C%m%d");

	pqxx::connection c(dbConnect);
	pqxx::read_transaction txn(c);
	pqxx::result r = txn.exec("SELECT referrer, host(ip), sum(cnt) AS cnt FROM tuples.temp_" + date_oss.str()
						+ " GROUP BY host(ip),referrer");

	std::ofstream file_db;
	file_db.open("db_counts.csv");
	for (auto row: r) {
		file_db << row[0].as<string>() << "," << row[1].as<string>() << "," << row[2].as<string>() << endl;
	}
	file_db.close();
	exit(0);
}

void send_msgs(const string& sendToSocket, const string& dbConnect, const int n_msgs){
	zmqpp::context_t context;
	context.set(zmqpp::context_option::io_threads, 2);
	zmqpp::socket_t  sender(context, zmqpp::socket_type::push);
	sender.set(zmqpp::socket_option::linger, 1000); // 1 second
	try {
	sender.connect(sendToSocket);
	cout << "sender thread socket: " << sendToSocket << endl;
	} catch (zmqpp::zmq_internal_exception &e){
		cout << "Exception: " << e.what() << endl;
		sender.close();
		context.terminate();
		return;
	}

	strings_pair_counter_map bidTuplesCounter;

	pqxx::connection c(dbConnect);
	pqxx::read_transaction txn(c);
	pqxx::result r = txn.exec("SELECT referrer, host(ip) FROM tuples.ip_ref_" + FLAGS_day + " LIMIT " + std::to_string(n_msgs) );
	// cout << "Test queries loaded, starting latency test" << endl;
	uint32_t reqID = 1;
	struct timespec t0, t1;
	struct timespec ts0, ts1;
	timespec tspec_sleep = {0, 0};
	int msg_time = 1e2; // In nanoseconds
	string referrer, ip;
	clock_gettime(CLOCK_MONOTONIC_RAW, &t0);
	for (auto row: r) {
		clock_gettime(CLOCK_MONOTONIC_RAW, &ts0);
		zmqpp::message_t bid_req;
		referrer = row[0].as<string>();
		ip = row[1].as<string>();
		bid_req << reqID << referrer << ip;
		sender.send(bid_req, true);
		reqID++;
		bidTuplesCounter[strings_pair(referrer,ip)]++;
		clock_gettime(CLOCK_MONOTONIC_RAW, &ts1);
		int t_sleep = msg_time-time_diff(ts0,ts1);
		if (t_sleep>0){
			tspec_sleep.tv_nsec = t_sleep;
			clock_nanosleep(CLOCK_MONOTONIC, 0, &tspec_sleep, NULL);
		}
	}
	sender.close();
	context.terminate();

	std::vector<std::pair<strings_pair, unsigned int>> vector( bidTuplesCounter.begin(), bidTuplesCounter.end() );

	// Sort the vector according to the count in descending order.
	std::sort( vector.begin(), vector.end(),
						[]( const auto & lhs, const auto & rhs )
						{ return lhs.second > rhs.second; } );

	std::ofstream file_input;
	file_input.open("input_counts.csv");
	for ( const auto & it : vector){
		file_input << it.first.first << "," << it.first.second << "," << it.second << endl;
	}
	file_input.close();

	clock_gettime(CLOCK_MONOTONIC_RAW, &t1);
	cout << "Test duration: " << time_diff(t0, t1)/1e9 << endl;
	cout << "# of messages sent: " << std::dec << n_msgs << endl;
}


long time_diff(const timespec t0, const timespec t1){
	long sec = t1.tv_sec - t0.tv_sec;
	long nsec = t1.tv_nsec - t0.tv_nsec;
	if (nsec<0){
		nsec = ONEEXPNINE - nsec;
		sec--;
	}
	return sec*ONEEXPNINE + nsec;
}
