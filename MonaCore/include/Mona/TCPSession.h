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

#pragma once

#include "Mona/Mona.h"
#include "Mona/Session.h"
#include "Mona/TCPClient.h"

namespace Mona {

struct TCPSession : Session, private TCPClient, virtual Object {
	void connect(const shared<Socket>& pSocket);

	UInt32 timeout() const { return _timeout; }

	const shared<Socket>&	socket() { return TCPClient::socket(); }

	void send(const Packet& packet);

	template<typename RunnerType>
	void send(const shared<RunnerType>& pRunner)  {
		// No check "died" here because send must be possible on kill, check died just for Writer new message creation (pRunner new creation)
		Exception ex;
		bool success;
		AUTO_ERROR(success = api.threadPool.queue(ex, pRunner, _sendingTrack), name());
		if (!success)
			kill(ex.cast<Ex::Intern>() ? ERROR_RESOURCE : ERROR_SOCKET); // no more thread available? TCP reliable! => disconnection!
	}

	virtual	void kill(Int32 error=0, const char* reason = NULL);

protected:
	/*!
	Subscribe to this event to receive data in child session, or overloads newDecoder() function */
	TCPClient::OnData&		onData;

	TCPSession(Protocol& protocol);

	virtual void onParameters(const Parameters& parameters);
	virtual bool manage();
	
private:
	void setSocketParameters(const Parameters& parameters);

	UInt32			_timeout;
	UInt16			_sendingTrack;
};



} // namespace Mona
