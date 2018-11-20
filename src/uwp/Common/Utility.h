#pragma once

#include <chrono>
#include <ctime>
#include <string>

namespace uwp {

Platform::String^ CharToString(const char * char_array);
double ClockToMilliseconds(clock_t ticks);

}
