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

#include "data-functions.h"

void queue_consumer(MessagesQueue *const requests_queue,
										const string& dbConnect, const int max_msgs){
	strings_pair_counter_map bidTuplesCounter;
	std::shared_ptr<strings_pair> ref_ip_ptr;
	string buffer;
	int n_msgs = 0;

	// pqxx::connection c(dbConnect);
	// pqxx::work work(c);

	while( ! boost::this_thread::interruption_requested() ){
		ref_ip_ptr = std::shared_ptr<strings_pair>(requests_queue->pop_front_block(1));
		if (ref_ip_ptr){
			bidTuplesCounter[*ref_ip_ptr]++; // WARNING: not thread safe writing when a new tuple is provided as the map may be rehashed
			n_msgs++;
		}
		if(n_msgs>max_msgs || boost::this_thread::interruption_requested()){
			auto t = std::time(nullptr);
			auto tm = *std::localtime(&t);

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
				if(!pgres){
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
				if(!pgres){
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
			if(!pgres){
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
