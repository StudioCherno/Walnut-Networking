#pragma once

#include <string>

namespace Walnut::Utils {

	bool IsValidIPAddress(std::string_view ipAddress);

	// Platform-specific implementations
	std::string ResolveHostName(std::string_view name);

}