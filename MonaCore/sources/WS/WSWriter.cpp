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

#include "Mona/WS/WSWriter.h"
#include "Mona/Logs.h"


using namespace std;


namespace Mona {

void WSWriter::closing(Int32 error, const char* reason) {
	if (error < 0)
		return;
	BinaryWriter& writer(*write(WS::TYPE_CLOSE));
	writer.write16(WS::ErrorToCode(error));
	if(reason)
		writer.write(reason);
}

void WSWriter::flushing() {
	for (auto& pSender : _senders)
		_session.send(pSender);
	_senders.clear();
}


void WSWriter::writeRaw(DataReader& reader) {
	UInt8 type;
	if (!reader.readNumber(type)) {
		ERROR("WebSocket content required at less a WS number type");
		return;
	}
	StringWriter writer(write(WS::Type(type))->buffer());
	reader.read(writer);
}

DataWriter& WSWriter::writeInvocation(const char* name) {
	DataWriter& invocation(writeJSON());
	invocation.writeString(name,strlen(name));
	return invocation;
}


bool WSWriter::beginMedia(const string& name, const Parameters& parameters) {
	writeJSON().writeString(EXPAND("@publishing"));
	return true;
}

bool WSWriter::writeAudio(UInt16 track, const Media::Audio::Tag& tag, const Packet& packet, bool reliable) {
	if(track)
		tag.pack(write(WS::TYPE_BINARY, packet)->write8(tag.packSize() + 2)).write16(track);
	else
		tag.pack(write(WS::TYPE_BINARY, packet)->write8(tag.packSize()));
	return true;
}

bool WSWriter::writeVideo(UInt16 track, const Media::Video::Tag& tag, const Packet& packet, bool reliable) {
	BinaryReader reader(packet.data(), packet.size());
	do {
		UInt32 size = tag.codec == Media::Video::CODEC_H264 ? reader.read32() : reader.available();
		if (size > reader.available())
			size = reader.available();

		BinaryWriter& writer(*write(WS::TYPE_BINARY, Packet(packet, reader.current(), size)));
		if (reader.position() < 5) {
			if (track)
				tag.pack(writer.write8(tag.packSize() + 2)).write16(track);
			else
				tag.pack(writer.write8(tag.packSize()));
		} else
			writer.write8(0); // no need to repeat tag!

		reader.next(size);
	} while (reader.available());
	return true;
}

bool WSWriter::writeData(UInt16 track, Media::Data::Type type, const Packet& packet, bool reliable) {
	// Always JSON
	// binary => Audio or Video
	// JSON => Data from server/publication
	DataWriter& writer(writeJSON(type, packet));
	// @ => Come from publication (to avoid confusion with message from server write by user)
	writer.writeString(EXPAND("@"));
	// Always write track for Data to avoid to confuse it with a real number argument if packet is already JSON
	writer.writeNumber(track);
	return true;
}

bool WSWriter::writeProperties(const Media::Properties& properties) {
	// Necessary TEXT(JSON) transfer (to match data publication)
	writeJSON(properties[Media::Data::TYPE_JSON]).writeString(EXPAND("@properties"));
	return true;
}

void WSWriter::endMedia(const string& name) {
	writeJSON().writeString(EXPAND("@unpublishing"));
}


} // namespace Mona
