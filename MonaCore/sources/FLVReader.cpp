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

#include "Mona/FLVReader.h"
#include "Mona/ADTSReader.h"
#include "Mona/AMFReader.h"
#include "Mona/Logs.h"

using namespace std;

namespace Mona {


UInt8 FLVReader::ReadMediaHeader(const UInt8* data, UInt32 size, Media::Audio::Tag& tag) {
	BinaryReader reader(data, size);
	if (!reader) {
		tag.isConfig = true; // audio end
		return 0; // keep other tag properties unchanged
	}
	 // empty packet => initialize isConfig = false (change everytime)
	UInt8 codecs(reader.read8());
	tag.codec = Media::Audio::Codec(codecs >> 4);
	if (reader.available() && tag.codec == Media::Audio::CODEC_AAC) {
		tag.isConfig = reader.read8() ? false : true;
		if (tag.isConfig) // Use rate here of AAC config rather previous!
			ADTSReader::ReadConfig(reader.current(), reader.available(), tag.rate, tag.channels);
	} else
		tag.isConfig = false;
	if (!tag.rate)
		tag.rate = UInt32(5512.5*pow(2, ((codecs & 0x0C) >> 2)));
	if(!tag.channels)
		tag.channels = (codecs & 0x01) + 1;
	return reader.position();
}

UInt8 FLVReader::ReadMediaHeader(const UInt8* data, UInt32 size, Media::Video::Tag& tag) {
	BinaryReader reader(data, size);
	if (!reader)
		return 0; // keep other tag properties unchanged
	UInt8 codecs(reader.read8());
	tag.codec = Media::Video::Codec(codecs & 0x0F);
	tag.frame = Media::Video::Frame((codecs & 0xF0) >> 4);
	if (tag.codec != Media::Video::CODEC_H264)
		return 1;
	if (!reader.read8())
		tag.frame = Media::Video::FRAME_CONFIG;
	tag.compositionOffset = reader.read24();
	return 5;
}

UInt32 FLVReader::ReadAVCConfig(const UInt8* data, UInt32 size, Buffer& buffer) {
	BinaryReader reader(data, size);
	
	reader.next(5); // skip avcC version 1 + 3 bytes of profile, compatibility, level + 1 byte xFF

	// SPS and PPS
	BinaryWriter writer(buffer);
	UInt8 count(reader.read8() & 0x1F);
	bool isPPS(false);
	while (reader.available() >= 2 && count--) {
		size = reader.read16();
		if (size > reader.available())
			size = reader.available();
		writer.write32(size).write(reader.current(), size);
		reader.next(size);
		if (!count) {
			if (isPPS)
				break;
			count = reader.read8(); // PPS now!
			isPPS = true;
		}
	}
	return reader.position();
}

UInt32 FLVReader::parse(const Packet& packet, Media::Source& source) {

	BinaryReader reader(packet.data(), packet.size());

	if (_begin) {
		if (!_size) {
			for (;;) {
				if (reader.available() < 9)
					return reader.available();
				if (reader.read24() == 0x464C56)
					break;
				if (!_syncError) {
					WARN("FLVReader signature not found");
					_syncError = true;
				}
			}
			_syncError = false;
			reader.next(2);
			_size = reader.read32();
			if (_size > 9)
				_size -= 4;
			else
				_size = 5;
		}
		_begin = false;
	}

	while (reader.available()) {

		if (!_type) {
			if (reader.available() < _size) {
				_size -= reader.available();
				return 0;
			}
			reader.next(_size-1);
			_type = (AMF::Type)reader.read8();
			_size = 0;
		}
		if (!_size) {
			if (reader.available() < 3)
				return reader.available();
			_size = reader.read24()+7;
		}

		if (reader.available() < _size)
			return reader.available();

		// Here reader.available()>=_size
		switch (_type) {
			case AMF::TYPE_VIDEO: {
				Media::Video::Tag tag;
				tag.time = reader.read24() | (reader.read8() << 24);
				UInt16 track((UInt16)reader.read24());
				_size -= 7;
				Packet video(packet, reader.current(), _size);
				video += ReadMediaHeader(video.data(), video.size(), tag);
				if (tag.codec == Media::Video::CODEC_H264 && tag.frame==Media::Video::FRAME_CONFIG) {
					shared<Buffer> pBuffer(new Buffer());
					video  += ReadAVCConfig(video.data(), video.size(), *pBuffer);
					source.writeVideo(track, tag, Packet(pBuffer));
				}
				if(video) // because if was just a config packet, there is no more data!
					source.writeVideo(track, tag, video);
				break;
			}
			case AMF::TYPE_AUDIO: {
				Media::Audio::Tag tag;
				tag.time = reader.read24() | (reader.read8() << 24);
				UInt16 track((UInt16)reader.read24());
				_size -= 7;
				tag.channels = _channels;
				tag.rate = _rate;
				Packet audio(packet, reader.current(), _size);
				source.writeAudio(track, tag, audio += ReadMediaHeader(audio.data(), audio.size(), tag));
				if(tag.isConfig) {
					_channels = tag.channels;
					_rate = tag.rate;
				}
				break;
			}
			case AMF::TYPE_DATA: {
				reader.next(4); // Time!
				UInt16 track((UInt16)reader.read24());
				_size -= 7;
				if (memcmp(reader.current(), EXPAND("\x02\x00\x0AonMetaData")) == 0) {
					AMFReader amf(reader.current() + 13, _size - 13);
					if(amf.available())
						source.writeProperties(track, amf);
				} else
					source.writeData(track, Media::Data::TYPE_AMF, Packet(packet, reader.current(), _size));
				break;
			}
			default:
				WARN("FLVReader with an unknown AMF type ", String::Format<UInt8>("%.2x", _type));
		}
	
		reader.next(_size);
		_type = AMF::TYPE_EMPTY;
		_size = 5; // footer + type
	}
	
	return 0;
}

void FLVReader::onFlush(const Packet& packet, Media::Source& source) {
	_begin = true;
	_size = 0;
	_type = AMF::TYPE_EMPTY;
	_rate = _channels = 0;
	_syncError = false;
	MediaReader::onFlush(packet, source);
}

} // namespace Mona
