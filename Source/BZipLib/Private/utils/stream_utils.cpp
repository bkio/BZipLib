//Petr Benes - https://bitbucket.org/wbenny/ziplib

#include "utils/stream_utils.h"

namespace utils {

	void stream::copy(std::istream& from, std::ostream& to, size_t bufferSize)
	{
		std::vector<char> buff(bufferSize);

		do
		{
			from.read(buff.data(), buff.size());
			to.write(buff.data(), from.gcount());
		} while (static_cast<size_t>(from.gcount()) == buff.size());
	}
}
