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

#ifndef _NAMELES_DATA_FUNCTIONS_
#define _NAMELES_DATA_FUNCTIONS_

#include <string>
#include <iostream>
#include <iomanip>
#include <ctime>
#include <sstream>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <utility>
#include <libpq-fe.h>
//#include <pqxx/pqxx>
#include <zmqpp/zmqpp.hpp>

#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <memory>
#include <unordered_map>
#include <boost/thread.hpp>


using std::cout;
using std::endl;
using std::string;


template <class T> class FifoQueue: private std::queue<std::shared_ptr<T>> {
  public:
    void push_back(std::shared_ptr<T> msg);
    std::shared_ptr<T> pop_front_block(const int& timeout_minutes);
  private:
    std::mutex _mutex;
    std::condition_variable _cond_var;
};

template <class T>
void FifoQueue<T>::push_back(std::shared_ptr<T> msg){
  std::unique_lock<std::mutex> _lock(_mutex);
  std::queue<std::shared_ptr<T>>::push(msg);
  _lock.unlock();
  _cond_var.notify_one();
};

template <class T>
std::shared_ptr<T> FifoQueue<T>::pop_front_block(const int& timeout_minutes){
  std::unique_lock<std::mutex> _lock(_mutex);
  while (std::queue<std::shared_ptr<T>>::empty()){
    if (_cond_var.wait_for(_lock, std::chrono::minutes(timeout_minutes)) == std::cv_status::timeout)
        return nullptr;
  }
  std::shared_ptr<T> msg = std::queue<std::shared_ptr<T>>::front();
  std::queue<std::shared_ptr<T>>::pop();
  return msg;
};

typedef std::pair<std::string, std::string> strings_pair;
typedef FifoQueue<strings_pair> MessagesQueue;

struct StringPairHash {
 std::size_t operator()(const strings_pair& k) const
 {
     return std::hash<std::string>()(k.first) ^
            (std::hash<std::string>()(k.second) << 1);
 }
};

struct StringPairEqual {
 bool operator()(const strings_pair& lhs, const strings_pair& rhs) const
 {
    return lhs.first == rhs.first && lhs.second == rhs.second;
 }
};

typedef std::unordered_map<strings_pair, unsigned int, StringPairHash, StringPairEqual> strings_pair_counter_map;


void queue_consumer(MessagesQueue *const requests_queue,
										const string& dbConnect, const int max_msgs);
void receive_msgs(MessagesQueue *const requests_queue,
									const string& receiveFromSocket);

#endif // _NAMELES_DATA_FUNCTIONS_
