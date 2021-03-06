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

#include "Mona/Publication.h"
#include "Mona/MapWriter.h"
#include "Mona/Logs.h"

// #include "faad.h"
// #include "neaacdec.h"


using namespace std;



namespace Mona {

// NeAACDecHandle _aac = NULL;
// FILE *_fout=NULL;

Publication::Publication(const string& name): _latency(0),
	audios(_audios), videos(_videos), datas(_datas), _lostRate(_byteRate),
	_running(false),_new(false), _newProperties(false), _newLost(false), _name(name) {
	DEBUG("New publication ",name);
}

Publication::~Publication() {
	stopRecording();
	// delete _listeners!
	if (!subscriptions.empty())
		CRITIC("Publication ",_name," with subscribers is deleting")
	if (_running)
		CRITIC("Publication ",_name," running is deleting")
	DEBUG("Publication ",_name," deleted");
}

void Publication::reportLost(UInt32 lost) {
	if (!lost) return;
	_newLost = true;
	_lostRate += lost;
	_audios.lostRate += UInt32(lost*_audios.byteRate / _byteRate);
	_videos.lostRate += UInt32(lost*_videos.byteRate / _byteRate);
	_datas.lostRate += UInt32(lost*_datas.byteRate / _byteRate);
}

void Publication::reportLost(Media::Type type, UInt32 lost) {
	if (!lost) return;
	switch (type) {
		case Media::TYPE_AUDIO: _audios.lostRate += lost; break;
		case Media::TYPE_VIDEO:
			_videos.lostRate += lost;
			for (auto& it : _videos)
				it.second.waitKeyFrame = true;
			break;
		case Media::TYPE_DATA: _datas.lostRate += lost; break;
		default: return reportLost(lost);
	}
	_newLost = true;
	_lostRate += lost;
}

void Publication::reportLost(Media::Type type, UInt16 track, UInt32 lost) {
	if (!lost) return;
	switch (type) {
		case Media::TYPE_AUDIO: {
			const auto& it = _audios.find(track);
			if (it == _audios.end())
				return;
			_audios.lostRate += lost;
			break;
		}
		case Media::TYPE_VIDEO: {
			const auto& it = _videos.find(track);
			if (it == _videos.end())
				return;
			_videos.lostRate += lost;
			it->second.waitKeyFrame = true;
			break;
		}
		case Media::TYPE_DATA:
			if (track >= _datas.size())
				return;
			_datas.lostRate += lost;
			break;
		default:
			reportLost(lost);
			return;
	}
	_newLost = true;
	_lostRate += lost;
}


void Publication::startRecording(MediaFile::Writer& recorder, bool append) {
	stopRecording();
	_pRecording.reset(new Subscription(recorder, false));
	NOTE("Start ", _name, "=>", recorder.path.name(), " recording");
	_pRecording->pPublication = this;
	_pRecording->setBoolean("append",append);
	((set<Subscription*>&)subscriptions).emplace(_pRecording.get());
	recorder.start(); // start MediaFile::Writer before subscription!
}

void Publication::stopRecording() {
	if (!_pRecording)
		return;
	NOTE("Stop ", _name, "=>", ((MediaFile::Writer&)_pRecording->target).path.name(), " recording");
	((set<Subscription*>&)subscriptions).erase(_pRecording.get());
	_pRecording->pPublication = NULL;
	delete &_pRecording->target;
	_pRecording.reset();
}

MediaFile::Writer* Publication::recorder() {
	if (!_pRecording)
		return NULL;
	if (_pRecording->ejected())
		_pRecording->reset();// to reset ejected and allow a MediaFile::Writer::start() manual to retry recording!
	return (MediaFile::Writer*)&_pRecording->target;
}

void Publication::start(MediaFile::Writer* pRecorder, bool append) {
	if (!_running) {
		_running = true;
		INFO("Publication ", _name, " started");
	}
	if(pRecorder)
		startRecording(*pRecorder, append);
	// no flush here, wait first flush or first media to allow to subscribe to onProperties event for a publisher!
}

void Publication::reset() {
	if (!_running)
		return;
	INFO("Publication ", _name, " reseted");
	_audios.clear();
	_videos.clear();
	_datas.clear();

	// Erase track metadata just!
	auto it = begin();
	while(it!=end()) {
		size_t point = it->first.find('.');
		if (point != string::npos && String::ToNumber(it->first.data(), point, point))
			erase((it++)->first);
		else
			++it;
	}
	_newProperties = false;

	for (auto& it : subscriptions) {
		if (it->pPublication == this || !it->pPublication) // If subscriber is subscribed
			it->reset();
	}
	
	onEnd();
}

void Publication::stop() {
	if(!_running)
		return; // already done

	stopRecording();

	_running=false;

	_audios.clear();
	_videos.clear();
	_datas.clear();
	Media::Properties::clear();

	_latency = 0;
	_new = _newProperties = false;


	INFO("Publication ", _name, " stopped")
	for(auto& it : subscriptions) {
		if (it->pPublication == this || !it->pPublication) // If subscriber is subscribed
			it->reset(); // call writer.endMedia on subscriber side and do the flush!
	}

	onFlush();
	onEnd();
}

void Publication::flush() {
	if (!_running) {
		ERROR("Publication flush called on publication ", _name, " stopped");
		return;
	}
	if (_newProperties) {
		_newProperties = false;
		// Logs before subscription logs!
		if (*this)
			INFO("Write ", _name, " publication properties")
		else
			INFO("Clear ", _name, " publication properties");
		for (auto& it : subscriptions) {
			if (it->pPublication == this || !it->pPublication) // If subscriber is subscribed
				it->writeProperties(*this);
		}
		onProperties(*this);
	} else if (!_new)
		return;
	_new = false;

	if(_newLost) {
		double lost;
		if((lost = _audios.lostRate))
			INFO(String::Format<double>("%.2f",lost * 100),"% audio lost on publication ",_name);
		if((lost = _videos.lostRate))
			INFO(String::Format<double>("%.2f", lost * 100),"% video lost on publication ",_name);
		if((lost = _datas.lostRate))
			INFO(String::Format<double>("%.2f", lost * 100),"% data lost on publication ",_name);
		_newLost = false;
	}

	for (auto& it : subscriptions) {
		if (it->pPublication == this || !it->pPublication) // If subscriber is subscribed OR is a target (pPublication==NULL)
			it->flush();
	}
	onFlush();
}


void Publication::writeAudio(UInt16 track, const Media::Audio::Tag& tag, const Packet& packet, UInt16 ping) {
	if (!_running) {
		ERROR("Audio packet on publication ", _name, " stopped");
		return;
	}
	if (ping)
		_latency = ping >> 1;
	_idleSince.update();

	TRACE("Audio time ", tag.time);

	/*if (!_fout)
	fopen_s(&_fout, "test.aac", "wb");
	fwrite(packet.data(), sizeof(UInt8), packet.size(), _fout);
	INFO(packet.size());

	//	INFO(tag.time, " ", packet.size())
	//	TRACE("Time Audio ",time)

	if(!_aac)
	_aac = NeAACDecOpen();

	unsigned long rate;
	UInt8 channels;

	if(packet) {
	if (tag.isConfig) {

	if (NeAACDecInit2(_aac, BIN packet.data(), packet.size(), &rate, &channels) != 0)
	bool error = true;
	} else {
	//	if (NeAACDecInit2(_aac, BIN _audioConfig.data(), _audioConfig.size(), &rate, &channels) != 0)
	//		bool error = true;

	NeAACDecFrameInfo info;
	void* decoded = NeAACDecDecode(_aac, &info, BIN packet.data(), packet.size());
	NOTE(info.samples);
	if(info.error || (info.bytesconsumed < packet.size()))
	bool error = true;
	}
	}*/
	
	
	// create track
	AudioTrack& audio = _audios[track];
	// save audio codec packet for future listeners
	if (tag.isConfig)
		DEBUG("Audio configuration received on publication ", _name);
	_byteRate += packet.size() + sizeof(tag);
	_audios.byteRate += packet.size() + sizeof(tag);
	_new = true;
	// TRACE("Audio ",tag.time);
	for (auto& it : subscriptions) {
		if (it->pPublication == this || !it->pPublication) // If subscriber is subscribed
			it->writeAudio(track, tag, packet);
	}
	onAudio(track, tag, packet);
	
	// Hold config packet after video distribution to avoid to distribute two times config packet if subscription call beginMedia
	if (tag.isConfig)
		audio.config.set(tag, packet);
}



void Publication::writeVideo(UInt16 track, const Media::Video::Tag& tag, const Packet& packet, UInt16 ping) {
	if (!_running) {
		ERROR("Video packet on publication ", _name, " stopped");
		return;
	}
	if (ping)
		_latency = ping >> 1;
	_idleSince.update();

	// create track
	VideoTrack& video = _videos[track];
	// save video codec packet for future listeners
	if (tag.frame == Media::Video::FRAME_CONFIG) {
		DEBUG("Video configuration received on publication ", _name);
	} else if (tag.frame == Media::Video::FRAME_KEY) {
		video.waitKeyFrame = false;
		onKeyFrame(track); // can add a new subscription for this publication!
	} else if (video.waitKeyFrame) {
		// wait one key frame (allow to rebuild the stream and saves bandwith without useless transfer
		INFO("Video frame dropped, ", _name, " waits a key frame");
		return;
	}

	_byteRate += packet.size() + sizeof(tag);
	_videos.byteRate += packet.size() + sizeof(tag);
	_new = true;
	// TRACE("Video ", tag.time);
	for (auto& it : subscriptions) {
		if (it->pPublication == this || !it->pPublication) // If subscriber is subscribed
			it->writeVideo(track, tag, packet);
	}
	onVideo(track, tag, packet);

	// Hold config packet after video distribution to avoid to distribute two times config packet if subscription call beginMedia
	if (tag.frame == Media::Video::FRAME_CONFIG)
		video.config.set(tag, packet);
}

void Publication::writeData(UInt16 track, Media::Data::Type type, const Packet& packet, UInt16 ping) {
	if (!_running) {
		ERROR("Data packet on '", _name, "' publication stopped");
		return;
	}
	if (ping)
		_latency = ping >> 1;
	_idleSince.update();

	unique_ptr<DataReader> pReader(Media::Data::NewReader(type, packet));
	if (pReader) {
		string handler;
		if (pReader->readString(handler) && handler == "@properties")
			return writeProperties(track, *pReader);
		pReader->reset();
	}

	// create track
	_datas[track];
	_byteRate += packet.size();
	_datas.byteRate += packet.size();
	_new = true;
	for (auto& it : subscriptions) {
		if (it->pPublication == this || !it->pPublication) // If subscriber is subscribed
			it->writeData(track, type, packet);
	}
	onData(track, type, packet);
}

void Publication::writeProperties(UInt16 track, DataReader& reader, UInt16 ping) {
	if (!_running) {
		ERROR("Properties on '", _name, "' publication stopped");
		return;
	}
	if (ping)
		_latency = ping >> 1;
	_idleSince.update();

	MapWriter<Parameters> writer(*this);
	writer.beginObject();
	writer.writePropertyName(to_string(track).c_str());
	reader.read(writer);
	writer.endObject();
}

} // namespace Mona
