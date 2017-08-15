/**
* Get  two std::time_t, 5 seconds apart, and show how they
* when used through localtime seemingly get messed up */
#include <ctime>
#include <iostream>
#include <thread>
#include <chrono>
 
int main()
{
   std::time_t t1 = std::time(nullptr);
   std::this_thread::sleep_for(std::chrono::seconds(5));
   std::time_t t2 = std::time(nullptr);
 
   // std::tm* points to static data that will be overwritten
   // for any new localtime call
   // 1) Print the original value before it is overwritten
   auto  t1_tm = *localtime(&t1);
   std::cout << "t1 (original)   :"<< std::asctime(&t1_tm) << '\n';
 
   // 2) Print t2 and t1 after t1 is overwritten by t2
   auto  t2_tm = *localtime(&t2);
   std::cout << "t1 (overwritten):"<< std::asctime(&t1_tm);
   std::cout << "t2              :"<< std::asctime(&t2_tm);
 
   return 0;
}
