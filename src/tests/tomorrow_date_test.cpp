#include <ctime>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <ratio>

int main()
{
  time_t base = 1490997600;
  for(int i = 0; i < 8; ++i) {
    time_t time = base - i * 24 * 60 * 60;
    std::cout << std::ctime(&time);
    auto tm = *std::localtime(&base);
    tm.tm_mday -= i;
    tm.tm_isdst = -1; // disable daylight saving time flag
    std::time_t tomorrow = std::mktime(&tm);
    std::cout << std::asctime(&tm);
    std::cout << std::ctime(&tomorrow);
    auto tp = std::chrono::system_clock::from_time_t(tomorrow);
    auto t_tp = std::chrono::system_clock::to_time_t(tp);
    std::cout << std::ctime(&t_tp) << std::endl;

    std::cout << std::endl;
  }
  return 0;
}
