#include "Result.hpp"

#include <comdef.h>

#include <System/String.hpp>

namespace ysn
{
	std::string ConvertHrToString(HRESULT hr)
	{
		_com_error err(hr);

	#ifdef UNICODE
		std::wstring w;
		w = err.ErrorMessage();
		std::string s = ysn::WStringToString(w);
	#else
		std::string s = err.ErrorMessage();
	#endif

		return s;
	}
} // namespace ysn
