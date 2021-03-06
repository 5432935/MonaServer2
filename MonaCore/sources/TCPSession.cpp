/*
This file is a part of MonaSolutions Copyright 2017
mathieu.poux[a]gmail.com
jammetthomas[a]gmail.com

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License received along this program for more
details (or else see http://www.gnu.org/licenses/).

*/

#include "Mona/TCPSession.h"
#include "Mona/Protocol.h"

using namespace std;


namespace Mona {

TCPSession::TCPSession(Protocol& protocol) : TCPClient(api.ioSocket), onData(TCPClient::onData), _sendingTrack(0), _timeout(protocol.getNumber<UInt32>("timeout") * 1000), Session(protocol, SocketAddress::Wildcard()) {}

void TCPSession::connect(const shared<Socket>& pSocket) {
	peer.setAddress(pSocket->peerAddress());
	setSocketParameters(protocol());

	onError = [this](const Exception& ex) { WARN(name(), ", ", ex); };
	onDisconnection = [this](const SocketAddress&) { kill(ERROR_SOCKET); };

	bool success;
	Exception ex;
	AUTO_ERROR(success = TCPClient::connect(ex, pSocket), name());
	if (success)
		onFlush = [this]() { flush(); }; // allow to signal end of congestion, and so was congestion so force flush (HTTPSession/HTTPFileSender uses it for example to continue to read a file)
	else
		kill(ERROR_SOCKET);
}

void TCPSession::setSocketParameters(const Parameters& parameters) {
	UInt32 bufferSize;
	Exception ex;
	if (parameters.getNumber("bufferSize", bufferSize)) {
		AUTO_ERROR((*this)->setRecvBufferSize(ex, bufferSize), name(), " receiving buffer setting");
		AUTO_ERROR((*this)->setSendBufferSize(ex = nullptr, bufferSize), name(), " sending buffer setting");
	}
	if (parameters.getNumber("recvBufferSize", bufferSize))
		AUTO_ERROR((*this)->setRecvBufferSize(ex = nullptr, bufferSize), name(), " receiving buffer setting");
	if (parameters.getNumber("sendBufferSize", bufferSize))
		AUTO_ERROR((*this)->setSendBufferSize(ex = nullptr, bufferSize), name(), " sending buffer setting");

	DEBUG(name(), " receiving buffer size of ", (*this)->recvBufferSize(), " bytes");
	DEBUG(name(), " sending buffer size of ", (*this)->sendBufferSize(), " bytes");
}

void TCPSession::onParameters(const Parameters& parameters) {
	Session::onParameters(parameters);
	if (parameters.getNumber("timeout", _timeout))
		_timeout *= 1000;
	setSocketParameters(parameters);
}

void TCPSession::send(const Packet& packet) {
	// No check "died" here because send must be possible on kill, check died just for Writer new message creation (pRunner new creation)
	Exception ex;
	bool success;
	AUTO_ERROR(success = api.threadPool.queue(ex, make_shared<TCPClient::Sender>(socket(), packet), _sendingTrack), name());
	if (!success)
		kill(ex.cast<Ex::Intern>() ? ERROR_RESOURCE : ERROR_SOCKET); // no more thread available? TCP reliable! => disconnection!
}

void TCPSession::kill(Int32 error, const char* reason) {
	if(died)
		return;

	// Stop reception
	onData = nullptr;

	Session::kill(error, reason); // onPeerDisconnection, before socket disconnection to allow possible last message

	// unsubscribe events before to avoid to get onDisconnection=>kill again on client.disconnect
	onDisconnection = nullptr;
	onError = nullptr;
	onFlush = nullptr;
	disconnect();
}

bool TCPSession::manage() {
	if (!Session::manage())
		return false;
	// TCP connection timeout to liberate useless socket ressource
	if (_timeout && socket()->recvTime().isElapsed(_timeout) && socket()->sendTime().isElapsed(_timeout)) {
		// Control sending and receiving for protocol like HTTP which can streaming always in the same way (sending), without never more request (receiving)
		INFO(name(), " timeout connection");
		kill(ERROR_IDLE);
	}
	return true;
}


} // namespace Mona
