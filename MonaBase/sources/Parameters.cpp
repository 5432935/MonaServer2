/*
This file is a part of MonaSolutions Copyright 2017
mathieu.poux[a]gmail.com
jammetthomas[a]gmail.com

This program is free software: you can redistribute it and/or
modify it under the terms of the the Mozilla Public License v2.0.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
Mozilla Public License v. 2.0 received along this program for more
details (or else see http://mozilla.org/MPL/2.0/).

*/

#include "Mona/Parameters.h"
#include "Mona/String.h"
#include "Mona/Exceptions.h"

using namespace std;

namespace Mona {

Parameters& Parameters::operator=(Parameters&& other) {
	if (other.empty()) {
		clear();
		return *this;
	}
	// clear self!
	_pMap.reset();
	_bytes = 0;
	onParamClear();
	// move data
	_pMap = std::move(other._pMap);
	_bytes = other._bytes;
	// clear other
	other._bytes = 0;
	other.onParamClear();
	// onChange!
	for (auto& it : *this)
		onParamChange(it.first, &it.second);
	return *this;
}

Parameters::ForEach Parameters::band(const std::string& prefix) {
	if (!_pMap)
		return ForEach();
	string end(prefix);
	end.back() = prefix.back() + 1;
	return ForEach(_pMap->lower_bound(prefix), _pMap->lower_bound(end));
}

bool Parameters::getString(const string& key, std::string& value) const {
	const char* temp = getParameter(key);
	if (!temp)
		return false;
	value.assign(temp);
	return true;
}

const char* Parameters::getString(const string& key, const char* defaultValue) const {
	const char* temp = getParameter(key);
	return temp ? temp : defaultValue;
}

bool Parameters::getBoolean(const string& key, bool& value) const {
	const char* temp = getParameter(key);
	if (!temp)
		return false;
	value = !String::IsFalse(temp); // otherwise considerate the value as true
	return true;
}

const char* Parameters::getParameter(const string& key) const {
	if (_pMap) {
		const auto& it = _pMap->find(key);
		if (it != _pMap->end())
			return it->second.c_str();
	}
	return onParamUnfound(key);
}

bool Parameters::erase(const string& key) {
	// erase
	if (!_pMap)
		return false;
	const auto& it(_pMap->find(key));
	if (it == _pMap->end())
		return true;
	if (_pMap->size() > 1) {
		_bytes -= it->second.size();
		onParamChange(it->first, NULL);
		_pMap->erase(it);
	} else
		clear();
	return true;
}


} // namespace Mona
