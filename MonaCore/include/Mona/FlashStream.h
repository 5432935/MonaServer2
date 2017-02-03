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
#include "Mona/AMFReader.h"
#include "Mona/FlashWriter.h"
#include "Mona/ServerAPI.h"
#include "Mona/Peer.h"

namespace Mona {

class FlashStream : public virtual Object {
public:
	typedef Event<void(UInt16 id, FlashWriter& writer)> ON(Start);
	typedef Event<void(UInt16 id, FlashWriter& writer)> ON(Stop);

	FlashStream(UInt16 id, ServerAPI& api, Peer& peer, bool amf0);
	virtual ~FlashStream();

	const UInt16	id;

	UInt32	bufferTime(UInt32 ms);
	UInt32	bufferTime() const { return _bufferTime; }

	// return flase if writer is closed!
	bool	process(AMF::Type type, UInt32 time, const Packet& packet, FlashWriter& writer, Net::Stats& netStats);

	void	reportLost(Media::Type type, UInt16 track, UInt32 lost) { if (_pPublication) _pPublication->reportLost(type, track, lost); }

	virtual void flush();

protected:

	bool			amf0;

	ServerAPI&		api;
	Peer&			peer;

private:
	void disengage(FlashWriter* pWriter);

	virtual void	messageHandler(const std::string& name, AMFReader& message, FlashWriter& writer, Net::Stats& netStats);
	virtual void	rawHandler(UInt16 type, const Packet& packet, FlashWriter& writer);
	virtual void	dataHandler(UInt32 timestamp, const Packet& packet);
	virtual void	audioHandler(UInt32 timestamp, const Packet& packet);
	virtual void	videoHandler(UInt32 timestamp, const Packet& packet);

	Publication*	_pPublication;
	Subscription*	_pSubscription;
	UInt32			_bufferTime;

	// Use class variable for media tag to use the previous value of its properties if packet is empty (can happen for audio for example)
	Media::Audio::Tag _audio;
	Media::Video::Tag _video;
	UInt32			  _media; // 1 byte for data type, track on 2 bytes, Media::Type on 1 byte
	UInt16			  _track;
};


} // namespace Mona