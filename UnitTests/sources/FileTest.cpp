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
#include "Mona/FileReader.h"
#include "Mona/FileWriter.h"

using namespace std;
using namespace Mona;


ADD_TEST(FileTest, File) {

	Exception ex;
	const char* name("temp.mona");

	{
		File file(name, File::MODE_WRITE);
		CHECK(file.write(ex, EXPAND("Salut")) && !ex);
		CHECK(file.size() == 5);
	}

	{
		CHECK(File(name, File::MODE_APPEND).write(ex, EXPAND("Salut")) && !ex);
		File file(name, File::MODE_READ);
		CHECK(file.size() == 10);
		char data[10];
		CHECK(file.read(ex, data, 20)==10 && !ex && memcmp(data, EXPAND("SalutSalut"))==0);
		CHECK(!file.write(ex, data, sizeof(data)) && ex && ex.cast<Ex::Intern>());
		ex = nullptr;
	}

	{
		File file(name, File::MODE_WRITE);
		CHECK(file.write(ex, EXPAND("Salut")) && !ex);
		CHECK(file.size() == 5);
		char data[10];
		CHECK(file.read(ex, data, sizeof(data))==-1 && ex && ex.cast<Ex::Intern>());
		ex = nullptr;
	}
}

static Handler		_Handler;
static ThreadPool	_ThreadPool;

ADD_TEST(FileTest, FileReader) {
	
	IOFile		io(_Handler, _ThreadPool);

	const char* name("temp.mona");
	Exception ex;

	FileReader reader(io);
	reader.onError = [](const Exception& ex) {
		FATAL_ERROR("FileReader, ", ex);
	};
	reader.onReaden = [&](shared<Buffer>& pBuffer, bool end) {
		if (pBuffer->size() > 3) {
			CHECK(pBuffer->size() == 5 && memcmp(pBuffer->data(), EXPAND("Salut")) == 0 && end);
			reader.close();
			reader.open(name).read(3);
		} else if (pBuffer->size() == 3) {
			CHECK(memcmp(pBuffer->data(), EXPAND("Sal")) == 0 && !end);
			reader.read(3);
		} else {
			CHECK(pBuffer->size() == 2 && memcmp(pBuffer->data(), EXPAND("ut")) == 0 && end);
			reader.close();
		}
	};
	reader.open(name).read();
	do {
		io.join();
	} while (_Handler.flush());
}

ADD_TEST(FileTest, FileWriter) {
	IOFile		io(_Handler, _ThreadPool);
	const char* name("temp.mona");
	Exception ex;
	Packet salut(EXPAND("Salut"));

	FileWriter writer(io);
	writer.onError = [](const Exception& ex) {
		FATAL_ERROR("FileWriter, ", ex);
	};
	writer.open(name).write(salut);
	io.join();
	CHECK(writer->size() == 5);
	writer.write(salut);
	io.join();
	CHECK(writer->size() == 10);
	writer.close();

	writer.open(name, true).write(salut);
	io.join();
	CHECK(writer->size() == 15);
	writer.close();

	CHECK(!_Handler.flush());
	CHECK(FileSystem::Delete(ex, name) && !ex);
}
