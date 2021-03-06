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

#include "Test.h"
#include "Mona/Stopwatch.h"
#include "Mona/Thread.h"

using namespace Mona;
using namespace std;

ADD_TEST(StopwatchTest, All) {
	
	Stopwatch sw;
	sw.start();
	Thread::Sleep(200);
	sw.stop();
	CHECK(180 < sw.elapsed() && sw.elapsed() < 220);

	sw.start();
	Thread::Sleep(100);
	sw.stop();
	CHECK(280 < sw.elapsed() && sw.elapsed() < 320);
	
	sw.restart();
	Thread::Sleep(200);
	sw.stop();
	CHECK(180 < sw.elapsed() && sw.elapsed() < 220);
}
