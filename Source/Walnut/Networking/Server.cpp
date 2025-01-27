#include "Server.h"

#include <iostream>
#include <chrono>

#include <spdlog/spdlog.h>

namespace Walnut {

	// Can only have one server instance per-process
	static Server* s_Instance = nullptr;

	Server::Server(int port)
		: m_Port(port)
	{
	}

	Server::~Server()
	{
		if (m_NetworkThread.joinable())
			m_NetworkThread.join();
	}

	void Server::Start()
	{
		if (m_Running)
			return;

		m_NetworkThread = std::thread([this]() { NetworkThreadFunc(); });
	}

	void Server::Stop()
	{
		m_Running = false;
	}

	void Server::NetworkThreadFunc()
	{
		s_Instance = this;
		m_Running = true;

		SteamDatagramErrMsg errMsg;
		if (!GameNetworkingSockets_Init(nullptr, errMsg))
		{
			OnFatalError(fmt::format("GameNetworkingSockets_Init failed: {}", errMsg));
			return;
		}

		m_Interface = SteamNetworkingSockets();

		// Start listening
		SteamNetworkingIPAddr serverLocalAddress;
		serverLocalAddress.Clear();
		serverLocalAddress.m_port = m_Port;

		SteamNetworkingConfigValue_t options;
		options.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged, (void*)Server::ConnectionStatusChangedCallback);

		// Try to start listen socket on port
		m_ListenSocket = m_Interface->CreateListenSocketIP(serverLocalAddress, 1, &options);

		if (m_ListenSocket == k_HSteamListenSocket_Invalid)
		{
			OnFatalError(fmt::format("Fatal error: Failed to listen on port {}", m_Port));
			return;
		}

		// Try to create poll group
		// TODO(Yan): should be optional, though good for groups which is probably the most common use case
		m_PollGroup = m_Interface->CreatePollGroup();
		if (m_PollGroup == k_HSteamNetPollGroup_Invalid)
		{
			OnFatalError(fmt::format("Fatal error: Failed to listen on port {}", m_Port));
			return;
		}

		std::cout << "Server listening on port " << m_Port << std::endl;

		while (m_Running)
		{
			PollIncomingMessages();
			PollConnectionStateChanges();
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}

		// Close all the connections
		std::cout << "Closing connections..." << std::endl;
		for (const auto& [clientID, clientInfo] : m_ConnectedClients)
		{
			m_Interface->CloseConnection(clientID, 0, "Server Shutdown", true);
		}
		
		m_ConnectedClients.clear();

		m_Interface->CloseListenSocket(m_ListenSocket);
		m_ListenSocket = k_HSteamListenSocket_Invalid;

		m_Interface->DestroyPollGroup(m_PollGroup);
		m_PollGroup = k_HSteamNetPollGroup_Invalid;
	}

	void Server::ConnectionStatusChangedCallback(SteamNetConnectionStatusChangedCallback_t* info) { s_Instance->OnConnectionStatusChanged(info); }

	void Server::OnConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* status)
	{
		// Handle connection state
		switch (status->m_info.m_eState)
		{
			case k_ESteamNetworkingConnectionState_None:
				// NOTE: We will get callbacks here when we destroy connections.  You can ignore these.
				break;

			case k_ESteamNetworkingConnectionState_ClosedByPeer:
			case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
			{
				// Ignore if they were not previously connected.  (If they disconnected
				// before we accepted the connection.)
				if (status->m_eOldState == k_ESteamNetworkingConnectionState_Connected)
				{
					// Locate the client.  Note that it should have been found, because this
					// is the only codepath where we remove clients (except on shutdown),
					// and connection change callbacks are dispatched in queue order.
					auto itClient = m_ConnectedClients.find(status->m_hConn);
					//assert(itClient != m_mapClients.end());

					// Either ClosedByPeer or ProblemDetectedLocally - should be communicated to user callback
					// User callback
					if (m_ClientDisconnectedCallback)
						m_ClientDisconnectedCallback(itClient->second);

					m_ConnectedClients.erase(itClient);
				}
				else
				{
					//assert(info->m_eOldState == k_ESteamNetworkingConnectionState_Connecting);
				}

				// Clean up the connection.  This is important!
				// The connection is "closed" in the network sense, but
				// it has not been destroyed.  We must close it on our end, too
				// to finish up.  The reason information do not matter in this case,
				// and we cannot linger because it's already closed on the other end,
				// so we just pass 0s.
				m_Interface->CloseConnection(status->m_hConn, 0, nullptr, false);
				break;
			}

			case k_ESteamNetworkingConnectionState_Connecting:
			{
				// This must be a new connection
				// assert(m_mapClients.find(info->m_hConn) == m_mapClients.end());

				// Try to accept incoming connection
				if (m_Interface->AcceptConnection(status->m_hConn) != k_EResultOK)
				{
					m_Interface->CloseConnection(status->m_hConn, 0, nullptr, false);
					std::cout << "Couldn't accept connection (it was already closed?)" << std::endl;
					break;
				}

				// Assign the poll group
				if (!m_Interface->SetConnectionPollGroup(status->m_hConn, m_PollGroup))
				{
					m_Interface->CloseConnection(status->m_hConn, 0, nullptr, false);
					std::cout << "Failed to set poll group" << std::endl;
					break;
				}

				// Retrieve connection info
				SteamNetConnectionInfo_t connectionInfo;
				m_Interface->GetConnectionInfo(status->m_hConn, &connectionInfo);

				// Register connected client
				auto& client = m_ConnectedClients[status->m_hConn];
				client.ID = (ClientID)status->m_hConn;
				client.ConnectionDesc = connectionInfo.m_szConnectionDescription;

				// User callback
				if (m_ClientConnectedCallback)
					m_ClientConnectedCallback(client);

				break;
			}

			case k_ESteamNetworkingConnectionState_Connected:
				// We will get a callback immediately after accepting the connection.
				// Since we are the server, we can ignore this, it's not news to us.
				break;

			default:
				break;
		}
	}

	void Server::PollConnectionStateChanges()
	{
		m_Interface->RunCallbacks();
	}

	void Server::PollIncomingMessages()
	{
		// Process all messages
		while (m_Running)
		{
			ISteamNetworkingMessage* incomingMessage = nullptr;
			int messageCount = m_Interface->ReceiveMessagesOnPollGroup(m_PollGroup, &incomingMessage, 1);
			if (messageCount == 0)
				break;

			if (messageCount < 0)
			{
				// messageCount < 0 means critical error?
				m_Running = false;
				return;
			}

			// assert(numMsgs == 1 && pIncomingMsg);

			auto itClient = m_ConnectedClients.find(incomingMessage->m_conn);
			if (itClient == m_ConnectedClients.end())
			{
				std::cout << "ERROR: Received data from unregistered client\n";
				continue;
			}

			if (incomingMessage->m_cbSize)
			{
				if (m_DataReceivedCallback)
					m_DataReceivedCallback(itClient->second, Buffer(incomingMessage->m_pData, incomingMessage->m_cbSize));
			}

			// Release when done
			incomingMessage->Release();
		}
	}

	void Server::SetClientNick(HSteamNetConnection hConn, const char* nick)
	{
		// Set the connection name, too, which is useful for debugging
		m_Interface->SetConnectionName(hConn, nick);
	}

	void Server::SetDataReceivedCallback(const DataReceivedCallback& function)
	{
		m_DataReceivedCallback = function;
	}

	void Server::SetClientConnectedCallback(const ClientConnectedCallback& function)
	{
		m_ClientConnectedCallback = function;
	}

	void Server::SetClientDisconnectedCallback(const ClientDisconnectedCallback& function)
	{
		m_ClientDisconnectedCallback = function;
	}

	void Server::SendBufferToClient(ClientID clientID, Buffer buffer, bool reliable)
	{
		m_Interface->SendMessageToConnection((HSteamNetConnection)clientID, buffer.Data, (ClientID)buffer.Size, reliable ? k_nSteamNetworkingSend_Reliable : k_nSteamNetworkingSend_Unreliable, nullptr);
	}

	void Server::SendBufferToAllClients(Buffer buffer, ClientID excludeClientID, bool reliable)
	{
		for (const auto& [clientID, clientInfo] : m_ConnectedClients)
		{
			if (clientID != excludeClientID)
				SendBufferToClient(clientID, buffer, reliable);
		}
	}

	void Server::SendStringToClient(ClientID clientID, const std::string& string, bool reliable)
	{
		SendBufferToClient(clientID, Buffer(string.data(), string.size()), reliable);
	}

	void Server::SendStringToAllClients(const std::string& string, ClientID excludeClientID, bool reliable)
	{
		SendBufferToAllClients(Buffer(string.data(), string.size()), excludeClientID, reliable);
	}

	void Server::KickClient(ClientID clientID)
	{
		m_Interface->CloseConnection(clientID, 0, "Kicked by host", false);
	}

	void Server::OnFatalError(const std::string& message)
	{
		std::cout << message << std::endl;
		m_Running = false;
	}

}
