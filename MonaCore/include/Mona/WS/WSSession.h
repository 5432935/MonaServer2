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
#include "Mona/TCPSession.h"
#include "Mona/WS/WSWriter.h"
#include "Mona/WS/WSDecoder.h"

namespace Mona {


struct WSSession : Session, virtual Object {
	WSSession(Protocol& protocol, TCPSession& session, const shared<WSDecoder>& pDecoder);

	WSWriter	writer;

	bool		manage();
	void		flush();
	void		kill(Int32 error=0, const char* reason = NULL);

protected:
	
	void		openSubscribtion(Exception& ex, std::string& stream, Writer& writer);
	void		closeSusbcription();
	
	void		openPublication(Exception& ex, std::string& stream);
	void		closePublication();

	/// \brief Read message and call method if needed
	/// \param packet Content message to read
	void		processMessage(Exception& ex, const Packet& message, bool isBinary=false);

private:

	WSDecoder::OnRequest	_onRequest;

	Publication*			_pPublication;
	Media::Video::Tag		_video;
	Media::Audio::Tag		_audio;
	UInt32					_media; // 1 byte for data type, 2 bytes for track, 7 bits for media type, 1 bit for "header"
	Subscription*			_pSubscription;
	TCPSession&				_httpSession;
};


} // namespace Mona
