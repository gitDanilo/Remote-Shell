#include <iostream>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include "ShellSession.h"

#define DEFAULT_PORT "4457"

int main(int argc, char** argv)
{
	WSADATA wsaData;
	SOCKET sockListen    = INVALID_SOCKET;
	SOCKET sockAccept    = INVALID_SOCKET;
	ADDRINFOA AddrInfo   = {};
	PADDRINFOA pAddrInfo = nullptr;
	ShellSession shellSession;
	DWORD dwReturn;
	int iReturn;

	iReturn = WSAStartup(MAKEWORD(2, 1), &wsaData);
	if (iReturn != 0)
	{
		std::cout << "WSAStartup failed with error: 0x" << std::hex << std::uppercase << iReturn << '.' << std::endl;
		return EXIT_FAILURE;
	}

	AddrInfo.ai_family = AF_INET;
	AddrInfo.ai_socktype = SOCK_STREAM;
	AddrInfo.ai_protocol = IPPROTO_TCP;
	AddrInfo.ai_flags = AI_PASSIVE;

	iReturn = getaddrinfo(nullptr, DEFAULT_PORT, &AddrInfo, &pAddrInfo);
	if (iReturn != 0)
	{
		std::cout << "getaddrinfo failed with error: 0x" << std::hex << std::uppercase << iReturn << '.' << std::endl;
		WSACleanup();
		return EXIT_FAILURE;
	}

	sockListen = socket(pAddrInfo->ai_family, pAddrInfo->ai_socktype, pAddrInfo->ai_protocol);
	if (sockListen == INVALID_SOCKET)
	{
		std::cout << "socket failed with error: 0x" << std::hex << std::uppercase << WSAGetLastError() << '.' << std::endl;
		freeaddrinfo(pAddrInfo);
		WSACleanup();
		return EXIT_FAILURE;
	}

	iReturn = bind(sockListen, pAddrInfo->ai_addr, static_cast<int>(pAddrInfo->ai_addrlen));
	if (iReturn == SOCKET_ERROR)
	{
		std::cout << "bind failed with error: 0x" << std::hex << std::uppercase << WSAGetLastError() << '.' << std::endl;
		freeaddrinfo(pAddrInfo);
		closesocket(sockListen);
		WSACleanup();
		return EXIT_FAILURE;
	}

	freeaddrinfo(pAddrInfo);

	iReturn = listen(sockListen, SOMAXCONN);
	if (iReturn == SOCKET_ERROR)
	{
		std::cout << "listen failed with error: 0x" << std::hex << std::uppercase << WSAGetLastError() << '.' << std::endl;
		closesocket(sockListen);
		WSACleanup();
		return EXIT_FAILURE;
	}

	do
	{
		std::cout << "Listening on port " << DEFAULT_PORT << "..." << std::endl;

		sockAccept = accept(sockListen, nullptr, nullptr);
		if (sockAccept == INVALID_SOCKET)
		{
			std::cout << "accept failed with error: 0x" << std::hex << std::uppercase << WSAGetLastError() << '.' << std::endl;
			closesocket(sockListen);
			WSACleanup();
			return EXIT_FAILURE;
		}

		dwReturn = shellSession.Init(sockAccept);
		if (dwReturn > 0)
		{
			std::cout << "Session initialization failed with error: 0x" << std::hex << std::uppercase << dwReturn << std::endl;
			shellSession.Close();
			closesocket(sockListen);
			WSACleanup();
			return EXIT_FAILURE;
		}

		dwReturn = shellSession.Wait();
		if (dwReturn > 0 && dwReturn != MAXDWORD)
		{
			std::cout << "Session error: 0x" << std::hex << std::uppercase << dwReturn << '.' << std::endl;
			shellSession.Close();
			closesocket(sockListen);
			WSACleanup();
			return EXIT_FAILURE;
		}

		shellSession.Close();

		std::cout << "End of session." << std::endl;
	} while (dwReturn == 0);

	std::cout << "Exiting..." << std::endl;

	closesocket(sockListen);
	WSACleanup();

	return EXIT_SUCCESS;
}