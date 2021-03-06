
#pragma once

#include "Mona/Mona.h"
#include "Mona/Publication.h"
#include "Mona/Client.h"

namespace Mona {

struct App : virtual Object {

	struct Client : virtual Object {
		Client(Mona::Client& client) : client(client) {}
		virtual ~Client() {}

		virtual void onAddressChanged(const SocketAddress& oldAddress) {}
		virtual bool onInvocation(Exception& ex, const std::string& name, DataReader& arguments, UInt8 responseType) { return false; }
		virtual bool onFileAccess(Exception& ex, Mona::File::Mode mode, Path& file, DataReader& arguments, DataWriter& properties) { return true; }

		virtual bool onPublish(Exception& ex, const Publication& publication) { return true; }
		virtual void onUnpublish(const Publication& publication) {}

		virtual bool onSubscribe(Exception& ex, const Subscription& subscription, const Publication& publication) { return true; }
		virtual void onUnsubscribe(const Subscription& subscription, const Publication& publication) {}

		Mona::Client& client;
	};

	App(const Parameters& configs) {}


	virtual void onHandshake(const std::string& protocol, const SocketAddress& address, const Parameters& properties, std::set<SocketAddress>& addresses) {}

	virtual App::Client* newClient(Exception& ex, Mona::Client& client, DataReader& parameters, DataWriter& response) {
		return NULL;
	}

	virtual void manage() {}
};

} // namespace Mona
