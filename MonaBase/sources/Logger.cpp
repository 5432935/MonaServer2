/*
Copyright 2014 Mona
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

This file is a part of Mona.
*/

#include "Mona/Logger.h"
#if defined(_WIN32)
#include <windows.h>
#endif
#include <iostream>
#include <mutex>


using namespace std;

namespace Mona {

#if defined(_WIN32)
#define FATAL_COLOR 12
#define CRITIC_COLOR 12
#define ERROR_COLOR 13
#define WARN_COLOR 14
#define NOTE_COLOR 10
#define INFO_COLOR 15
#define DEBUG_COLOR 7
#define TRACE_COLOR 8
#define BEGIN_CONSOLE_TEXT_COLOR(color) static HANDLE ConsoleHandle(GetStdHandle(STD_OUTPUT_HANDLE)); SetConsoleTextAttribute(ConsoleHandle, color)
#define END_CONSOLE_TEXT_COLOR			SetConsoleTextAttribute(ConsoleHandle, LevelColors[6])
#else
#define FATAL_COLOR "\033[01;31m"
#define	CRITIC_COLOR "\033[01;31m"
#define ERROR_COLOR "\033[01;35m"
#define WARN_COLOR "\033[01;33m"	
#define NOTE_COLOR "\033[01;32m"
#define INFO_COLOR "\033[01;37m"
#define DEBUG_COLOR "\033[0m"
#define TRACE_COLOR "\033[01;30m"
#define BEGIN_CONSOLE_TEXT_COLOR(color) cout << color
#define END_CONSOLE_TEXT_COLOR			cout << LevelColors[6]
#endif

#if defined(_WIN32)
static int			LevelColors[] = { FATAL_COLOR, CRITIC_COLOR, ERROR_COLOR, WARN_COLOR, NOTE_COLOR, INFO_COLOR, DEBUG_COLOR, TRACE_COLOR };
#else
static const char*  LevelColors[] = { FATAL_COLOR, CRITIC_COLOR, ERROR_COLOR, WARN_COLOR, NOTE_COLOR, INFO_COLOR, DEBUG_COLOR, TRACE_COLOR };
#endif

void Logger::log(LOG_LEVEL level, const Path& file, long line, const string& message) {
	BEGIN_CONSOLE_TEXT_COLOR(LevelColors[level - 1]);
	cout << file.name() << '[' << line << "] " << message;
	END_CONSOLE_TEXT_COLOR;
	cout << std::endl; // flush after color change, required especially over unix/linux
}

void Logger::dump(const string& header, const UInt8* data, UInt32 size) {
	if(!header.empty())
		cout.write(header.data(), header.size()).put('\n');
	cout.write(STR data, size).flush();
}

} // namespace Mona
