#include <thread>
#include <chrono>
#include "EzySocketTcpClient.h"
#include "../logger/EzyLogger.h"
#include "../pool/EzyAutoReleasePool.h"

EZY_NAMESPACE_START
namespace socket {

EzySocketTcpWriter::EzySocketTcpWriter() {
}

EzySocketTcpWriter::~EzySocketTcpWriter() {
}

void EzySocketTcpWriter::update(){
	int rs;
	int sentData;
    auto releasePool = pool::EzyAutoReleasePool::getInstance()->getPool();
	while (true){
		releasePool->releaseAll();
		if (!isRunning()) {
			return;
		}

		EzySocketData* sendData = socketPool->take();
		if (sendData){
			sentData = 0;
			this->toBufferData(sendData);
			const std::vector<char>& sendBuffer = encoder->getBuffer();

			while (true) {
				rs = send(socketClient, sendBuffer.data() + sentData, sendBuffer.size() - sentData, 0);
				//rs = write(mSocket, senderBuffer.data(), senderBuffer.size());
				if (rs > 0){
					sentData += rs;
					if (sentData < sendBuffer.size()){
						continue;
					}
					break;
				}
				else if (rs == 0){
#ifdef EZY_DEBUG
                    logger::log("server shutdown[2]");
#endif
					this->setRunning(false);
					return;
				}
				else{
#ifdef EZY_DEBUG
					logger::log("send error");
#endif
					this->setRunning(false);
					return;
				}
			}
		}
		else{
			this->setRunning(false);
			return;
		}
	}
}

/****/
EzySocketTcpReader::EzySocketTcpReader() {
}

EzySocketTcpReader::~EzySocketTcpReader(){
}

#define BUFFER_SIZE 102400 //100KB
void EzySocketTcpReader::update(){
	int rs;
	char dataBuffer[BUFFER_SIZE];
    auto releasePool = pool::EzyAutoReleasePool::getInstance()->getPool();
	while (true){
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
		releasePool->releaseAll();

		if (!isRunning()){
			break;
		}
		rs = recv(socketClient, dataBuffer, BUFFER_SIZE, 0);
		if (rs > 0){
#ifdef EZY_DEBUG
			logger::log("recvdata: %d",rs);
#endif
			this->recvData(dataBuffer, rs);
		}
		else if (rs == 0){
#ifdef EZY_DEBUG
			logger::log("server shutdown[1]");
#endif
			this->setRunning(false);
			break;
		}
		else{
			//error
#ifdef EZY_DEBUG
			logger::log("recv header error");
#endif
			this->setRunning(false);
			break;
		}
	}
}

/**/
EzySocketTcpClient::EzySocketTcpClient() {
	// TODO Auto-generated constructor stub
	socketClient = SYS_SOCKET_INVALID;

#ifdef USE_WINSOCK_2
	WORD wVersionRequested;
	WSADATA wsaData;
	wVersionRequested = MAKEWORD(2, 2);
	WSAStartup(wVersionRequested, &wsaData);
#endif
}

EzySocketTcpClient::~EzySocketTcpClient() {
#ifdef USE_WINSOCK_2
	WSACleanup();
#endif
}

void EzySocketTcpClient::closeSocket(){
	std::unique_lock<std::mutex> lk(socketMutex);
	if (socketClient != SYS_SOCKET_INVALID){
#ifdef USE_WINSOCK_2
		shutdown(mSocket, SD_BOTH);
		closesocket(mSocket);
#else
		shutdown(socketClient, SHUT_RDWR);
		close(socketClient);
#endif
		socketClient = SYS_SOCKET_INVALID;
	}
}

void EzySocketTcpClient::resetSocket() {
	std::unique_lock<std::mutex> lk(socketMutex);
	socketClient = SYS_SOCKET_INVALID;
}

void EzySocketTcpClient::createAdapter() {
	std::unique_lock<std::mutex> lk(clientMutex);
	socketReader = new EzySocketTcpReader();
	socketWriter = new EzySocketTcpWriter();
}

void EzySocketTcpClient::startAdapter(){
	std::unique_lock<std::mutex> lk(clientMutex);
	((EzySocketTcpWriter*)socketWriter)->socketClient = socketClient;
	socketWriter->start();
	((EzySocketTcpReader*)socketReader)->socketClient = socketClient;
	socketReader->start();
}


bool EzySocketTcpClient::connectThread(){
	addrinfo hints, *peer;
	memset(&hints, 0, sizeof(struct addrinfo));
#ifdef __linux
#if defined(ANDROID)
	hints.ai_flags = AI_PASSIVE;
#else
	hints.ai_flags = AI_ALL;
#endif
#else
	hints.ai_flags = AI_ALL;
#endif
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	char service[128];
	sprintf(service, "%d", port);
	if (int ret = getaddrinfo(host.c_str(), service, &hints, &peer) != 0){
#ifdef EZY_DEBUG
		logger::log("getaddrinfo failure %d", ret);
#endif
		return false;
	}

	for (auto _peer = peer; _peer; _peer = _peer->ai_next){
        socketClient = ::socket(_peer->ai_family, _peer->ai_socktype, _peer->ai_protocol);
		if (socketClient == SYS_SOCKET_INVALID){
#ifdef EZY_DEBUG
			logger::log("create socket failure");
#endif
			continue;
		}

		int rs = connect(socketClient, _peer->ai_addr, _peer->ai_addrlen);
		if (rs == 0){
			std::unique_lock<std::mutex> lk(clientMutex);
			if (socketReader && socketWriter){
				freeaddrinfo(peer);
				return true;
			}
		}	
#ifdef USE_WINSOCK_2
		closesocket(socketClient);
#else
		close(socketClient);
#endif	
	}

	freeaddrinfo(peer);
#ifdef EZY_DEBUG
	logger::log("connection failure 3");
#endif
	return false;
}

}
EZY_NAMESPACE_END
