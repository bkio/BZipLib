//Petr Benes - https://bitbucket.org/wbenny/ziplib

#pragma once
#include <iostream>
#include <vector>

namespace utils {
	class stream {

	public:
		static void copy(std::istream& from, std::ostream& to, size_t bufferSize = 1024 * 1024);
	};
}
