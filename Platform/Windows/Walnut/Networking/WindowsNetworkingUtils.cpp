#include "Walnut/Networking/NetworkingUtils.h"

#include "Walnut/Utils/StringUtils.h"

#include <WinSock2.h>
#include <ws2tcpip.h>

#include <vector>

namespace Walnut::Utils {

	std::string ResolveDomainName(std::string_view name)
	{
		// Adapted from example at https://learn.microsoft.com/en-us/windows/win32/api/ws2tcpip/nf-ws2tcpip-getaddrinfo
		// TODO(Yan): better error logging

		WSADATA wsaData;
		int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (iResult != 0)
		{
			printf("WSAStartup failed with %u\n", WSAGetLastError());
			return {};
		}

		bool hasPort = name.find(":") != std::string::npos;
		std::string domain, port;
		if (hasPort)
		{
			std::vector<std::string> domainAndPort = SplitString(name, ':');
			if (domainAndPort.size() != 2)
				return {};
			domain = domainAndPort[0];
			port = domainAndPort[1];
			name = domain;
		}

		addrinfo hints;
		ZeroMemory(&hints, sizeof(hints));
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;

		addrinfo* addressResult = NULL;
		DWORD dwRetval = getaddrinfo(name.data(), nullptr, &hints, &addressResult);
		if (dwRetval != 0)
		{
			printf("getaddrinfo failed with error: %d\n", dwRetval);
			WSACleanup();
			return {};
		}

		std::string ipAddressStr;
		for (addrinfo* ptr = addressResult; ptr != NULL; ptr = ptr->ai_next)
		{
			switch (ptr->ai_family)
			{
			case AF_UNSPEC:
				// Unspecified
				break;
			case AF_INET:
			{
				sockaddr_in* sockaddr_ipv4 = (sockaddr_in*)ptr->ai_addr;
				char* ipAddress = inet_ntoa(sockaddr_ipv4->sin_addr);
				ipAddressStr = ipAddress;
				break;
			}
			case AF_INET6:
			{
				const DWORD ipbufferlength = 46;
				char ipstringbuffer[ipbufferlength];
				DWORD actualIPBufferLength = ipbufferlength;
				LPSOCKADDR sockaddr_ip = (LPSOCKADDR)ptr->ai_addr;
				INT iRetval = WSAAddressToStringA(sockaddr_ip, (DWORD)ptr->ai_addrlen, nullptr, ipstringbuffer, &actualIPBufferLength);

				if (iRetval)
				{
					printf("WSAAddressToString failed with %u\n", WSAGetLastError());
					WSACleanup();
					return {};
				}

				ipAddressStr = std::string(ipstringbuffer, actualIPBufferLength);
			}
			}

		}

		freeaddrinfo(addressResult);
		WSACleanup();

		return hasPort ? (ipAddressStr + ":" + port) : ipAddressStr;
	}

}