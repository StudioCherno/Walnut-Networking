#pragma once

#include "Walnut/Core/Buffer.h"

#include <steam/steamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h>
#ifndef STEAMNETWORKINGSOCKETS_OPENSOURCE
#include <steam/steam_api.h>
#endif

#include <string>
#include <map>
#include <thread>
#include <functional>

namespace Walnut {

	class Client
	{
	public:
		enum class ConnectionStatus
		{
			Disconnected = 0, Connected, Connecting, FailedToConnect
		};
	public:
		using DataReceivedCallback = std::function<void(const Buffer)>;
		using ServerConnectedCallback = std::function<void()>;
		using ServerDisconnectedCallback = std::function<void()>;
	public:
		Client() = default;
		~Client();

		void ConnectToServer(const std::string& serverAddress);
		void Disconnect();

		//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		// Set callbacks for server events
		// These callbacks will be called from the network thread
		//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		void SetDataReceivedCallback(const DataReceivedCallback& function);
		void SetServerConnectedCallback(const ServerConnectedCallback& function);
		void SetServerDisconnectedCallback(const ServerDisconnectedCallback& function);

		//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		// Send Data
		//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		void SendBuffer(Buffer buffer, bool reliable = true);
		void SendString(const std::string& string, bool reliable = true);

		template<typename T>
		void SendData(const T& data, bool reliable = true)
		{
			SendBuffer(Buffer(&data, sizeof(T)), reliable);
		}

		//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		// Connection Status & Debugging
		//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		bool IsRunning() const { return m_Running; }
		ConnectionStatus GetConnectionStatus() const { return m_ConnectionStatus; }
		const std::string& GetConnectionDebugMessage() const { return m_ConnectionDebugMessage; }

		uint32_t GetID() const { return m_Connection; }
	private:
		void NetworkThreadFunc();
		void Shutdown();
	private:
		static void ConnectionStatusChangedCallback(SteamNetConnectionStatusChangedCallback_t* info);
		void OnConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* info);

		void PollIncomingMessages();
		void PollConnectionStateChanges();

		void OnFatalError(const std::string& message);
	private:
		std::thread m_NetworkThread;
		DataReceivedCallback m_DataReceivedCallback;
		ServerConnectedCallback m_ServerConnectedCallback;
		ServerDisconnectedCallback m_ServerDisconnectedCallback;

		ConnectionStatus m_ConnectionStatus = ConnectionStatus::Disconnected;
		std::string m_ConnectionDebugMessage;

		std::string m_ServerAddress, m_ServerIPAddress;
		bool m_Running = false;

		ISteamNetworkingSockets* m_Interface = nullptr;
		HSteamNetConnection m_Connection = 0;
	};

}