//Petr Benes - https://bitbucket.org/wbenny/ziplib

#pragma once
#include <cstdint>
#include <ctime>

namespace utils {
	class time {

	public:
		static void timestamp_to_datetime(time_t dateTime, uint16_t& date, uint16_t& time);
		static time_t datetime_to_timestamp(uint16_t date, uint16_t time);
	};
}