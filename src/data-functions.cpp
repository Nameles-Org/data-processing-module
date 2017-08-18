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

#include <Python.h>
#include <iomanip>
#include <ctime>
#include <ratio>
#include <chrono>
#include <cwchar>
#include <sstream>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <chrono>
#include <utility>
#include <boost/thread.hpp>
#include <libpq-fe.h>
#include <zmqpp/zmqpp.hpp>

#include "data-functions.h"

void queue_consumer(MessagesQueue *const requests_queue,
										const string& dbConnect, const int max_msgs){
	strings_pair_counter_map bidTuplesCounter;
	std::shared_ptr<strings_pair> ref_ip_ptr;
	string buffer;
	int n_msgs = 0;

	while( ! boost::this_thread::interruption_requested() ){
		ref_ip_ptr = std::shared_ptr<strings_pair>(requests_queue->pop_front_block(1));
		if (ref_ip_ptr){
			bidTuplesCounter[*ref_ip_ptr]++; // WARNING: not thread safe writing when a new tuple is provided as the map may be rehashed
			n_msgs++;
		}
		if(n_msgs>max_msgs || boost::this_thread::interruption_requested()){
			auto t = std::time(nullptr);
			auto tm = *std::localtime(&t);  // Always get the struct from localtime (copy the value). The pointer just gives a local buffer.

			std::ostringstream date_oss;
			date_oss << std::put_time(&tm, "%C%m%d");
			auto temp_table = "tuples.temp_" + date_oss.str();
			int retries = 0;
			PGconn * pgconn = NULL;
			PGresult * pgres = NULL;
			do{
				if(!pgconn){
					PQfinish(pgconn);
				}
				pgconn = PQconnectdb(dbConnect.c_str());
			} while(PQstatus(pgconn)==CONNECTION_BAD && retries++<5);
			if(PQstatus(pgconn)==CONNECTION_BAD){
				PQfinish(pgconn);
				cout << "Can't connect to postgreSQL" << endl;
				exit(-1);
			}
			buffer = "CREATE table IF NOT EXISTS " + temp_table + " ( ) INHERITS (tuples.ip_ref)";
			pgres = PQexec(pgconn, buffer.c_str());
			retries = 0;
			do{
				if(pgres){
					PQclear(pgres);
					pgres = NULL;
				}
				buffer = "COPY " + temp_table + " FROM stdin";
				pgres = PQexec(pgconn, buffer.c_str());
				if (PQresultStatus(pgres)==PGRES_COPY_IN){
      		for (auto it : bidTuplesCounter){
						buffer = it.first.second+"\t"+it.first.first+"\t"+std::to_string(it.second)+"\n";
						PQputCopyData(pgconn, buffer.c_str(), buffer.length());
    			}
					if(PQputCopyEnd(pgconn, NULL)<0){
						continue;
					}
				}
				if(pgres){
					PQclear(pgres);
					pgres = NULL;
				}
				pgres = PQgetResult(pgconn);
				// cout << PQresStatus(PQresultStatus(pgres)) << endl;
			} while(PQresultStatus(pgres)!=PGRES_COMMAND_OK && retries++<5);
			if(PQresultStatus(pgres)!=PGRES_COMMAND_OK){
				PQfinish(pgconn);
				cout << "Can't copy data to postgreSQL" << endl;
				exit(-1);
			}
			if(pgres){
				PQclear(pgres);
				pgres = NULL;
			}
			PQfinish(pgconn);
			bidTuplesCounter.clear();
			n_msgs = 0;
			// cout << boost::this_thread::get_id() << " hash table dumped to db & cleared" << endl;
		}
	}
	cout << boost::this_thread::get_id() << " ended" << endl;
}


// n_msgs defines the number of messages to signal for saving to database
void receive_msgs(MessagesQueue *const requests_queue,
									const string& receiveFromSocket){
	zmqpp::context_t context;
	context.set(zmqpp::context_option::io_threads, 2);
	zmqpp::socket_t  receiver(context, zmqpp::socket_type::pull);
	receiver.set(zmqpp::socket_option::linger, 1000); // 1 second
	receiver.set(zmqpp::socket_option::receive_timeout, 10000); // 10 seconds
	try {
		receiver.bind(receiveFromSocket);
	} catch (zmqpp::zmq_internal_exception &e){
		cout << "Exception: " << e.what() << endl;
		receiver.close();
		context.terminate();
		return;
	}
	cout << "receiver thread socket: " << receiveFromSocket << endl;

	zmqpp::message_t msg;
  string referrer, ip, buffer;
	// uint32_t reqID;

	while( ! boost::this_thread::interruption_requested() ){
		if (receiver.receive(msg)){
			// msg.get(reqID, 0);
			msg.get(referrer, 1);
			msg.get(ip, 2);
			// cout << reqID << " - " << referrer.c_str() << " - " << ip << endl;
			requests_queue->push_back(std::make_shared<strings_pair>(referrer,ip));
		}
		// else{
		// 	cout << "timeout " << boost::this_thread::interruption_requested() << endl;
		// }
	}

	receiver.close();
	context.terminate();
}

void compute_scores(const char* prog_name, const string& python_script_name, const string& notify_sockets){

	std::string str_prog_name(prog_name);
  std::wstring w_prog_name(str_prog_name.begin(),str_prog_name.end());
	wchar_t wc_prog_name[w_prog_name.length()+1];
	std::wcscpy(wc_prog_name, w_prog_name.c_str());

	std::wstring w_notify_sockets(notify_sockets.begin(), notify_sockets.end());
	wchar_t wc_notify_sockets[w_notify_sockets.length()+1];
	std::wcscpy(wc_notify_sockets, w_notify_sockets.c_str());

	while( ! boost::this_thread::interruption_requested() ){
		auto t = std::time(nullptr);
		auto tm = *std::localtime(&t);  // Always get the struct from localtime (copy the value). The pointer just gives a local buffer.
		wchar_t working_date[7];
		wcsftime(working_date, 7, L"%C%m%d\0", &tm);
		tm.tm_mday += 1;
		tm.tm_isdst = -1;
		tm.tm_hour = 0;
		tm.tm_min = 0;
		tm.tm_sec = 0;
		std::this_thread::sleep_until(std::chrono::system_clock::from_time_t(std::mktime(&tm)));

		  wchar_t wargv0[] = L"python_hello_world.py";
		  wchar_t wargv1[] = L"--date";
			// wargv2 is working_date
		  wchar_t wargv3[] = L"--socket";
		  // wargv4 is wc_notify_sockets
		  wchar_t *wargv[] = {&wargv0[0], &wargv1[0], &working_date[0], &wargv3[0], &wc_notify_sockets[0]};

		  Py_SetProgramName(wc_prog_name);  // optional but recommended

		  Py_Initialize();
		  PySys_SetArgv(2, &wargv[0]);
		  FILE *file = fopen(python_script_name.c_str(),"r");
		  PyRun_SimpleFile(file, python_script_name.c_str());
		  fclose(file);
		  Py_Finalize();
	}
}
