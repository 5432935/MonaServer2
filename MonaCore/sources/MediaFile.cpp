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

#include "Mona/MediaFile.h"
#include "Mona/Session.h"

using namespace std;

namespace Mona {


UInt32 MediaFile::Reader::Decoder::decode(shared<Buffer>& pBuffer, bool end) {
	DUMP_RESPONSE(_name.c_str(), pBuffer->data(), pBuffer->size(), _path);
	Packet packet(pBuffer);
	if (_pReader.unique())
		return 0;
	_pReader->read(packet, *this);
	if (end)
		_handler.queue(onEnd);
	return packet.size();
}

MediaFile::Reader::Reader(const Path& path, MediaReader* pReader, IOFile& io) :
	Media::Stream(type), io(io), path(path), _pReader(pReader) {
	_onFileError = [this](const Exception& ex) { Stream::stop(LOG_ERROR, ex); };
}

void MediaFile::Reader::start() {
	if (!_pSource) {
		Stream::stop<Ex::Intern>(LOG_ERROR, "call start(Media::Source& source) in first");
		return;
	}
	if (_pFile)
		return;
	shared<Decoder> pDecoder(new Decoder(io.handler, _pReader, path, _pSource->name()));
	pDecoder->onEnd = _onEnd = [this]() {  stop(); };
	pDecoder->onFlush = _onFlush = [this]() { _pSource->flush(); };
	pDecoder->onReset = _onReset = [this]() { _pSource->reset(); };
	pDecoder->onLost = _onLost = [this](Lost& lost) { lost.report(*_pSource); };
	pDecoder->onMedia = _onMedia = [this](Media::Base& media) { _pSource->writeMedia(media); };
	io.open(_pFile, path, pDecoder, nullptr, _onFileError);
	io.read(_pFile);
	INFO(description(), " starts");
}

void MediaFile::Reader::stop() {
	if (!_pFile)
		return;
	io.close(_pFile);
	// reset _pReader because could be used by different thread by new Socket and its decoding thread
	_pReader.reset(MediaReader::New(_pReader->format()));
	_onEnd = nullptr;
	_onFlush = nullptr;
	_onReset = nullptr;
	_onLost = nullptr;
	_onMedia = nullptr;
	INFO(description(), " stops");
}


MediaFile::Writer::Write::Write(const shared<string>& pName, IOFile& io, const shared<File>& pFile, const shared<MediaWriter>& pWriter) : Runner("MediaFileWrite"), _io(io), _pFile(pFile), pWriter(pWriter), _pName(pName),
	onWrite([this](const Packet& packet) {
		DUMP_REQUEST(_pName->c_str(), packet.data(), packet.size(), _pFile->path());
		_io.write(_pFile, packet);
	}) {
}

MediaFile::Writer::Writer(const Path& path, MediaWriter* pWriter, IOFile& io) :
	Media::Stream(TYPE_FILE), io(io), _writeTrack(0), _running(false), path(path), _pWriter(pWriter) {
	_onError = [this](const Exception& ex) { Stream::stop(LOG_ERROR, ex); };
}
 
void MediaFile::Writer::start() {
	_running = true;
}

bool MediaFile::Writer::beginMedia(const string& name, const Parameters& parameters) {
	if (!_running)
		return false; // Not started => no Log, just ejects
	// New media, so open the file to write here => overwrite by default, otherwise append if requested!
	io.open(_pFile, path, _onError, parameters.getBoolean<false>("append"));
	INFO(description(), " starts");
	_pName.reset(new string(name));
	return write<Write>();
}

void MediaFile::Writer::endMedia(const string& name) {
	write<EndWrite>();
	stop();
	start();
}

void MediaFile::Writer::stop() {
	_pName.reset();
	if(_pFile) {
		io.close(_pFile);
		INFO(description(), " stops");
	}
	_running = false;
	
}



} // namespace Mona
