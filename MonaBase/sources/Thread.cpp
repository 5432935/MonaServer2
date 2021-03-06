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


#include "Mona/Thread.h"
#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#if defined(__APPLE__)
#include <pthread.h>
#elif defined(_OS_BSD)
#include <pthread_np.h>
#else
#include <sys/prctl.h> // for thread name
#endif
#include <sys/syscall.h>
#endif
#include "Mona/Logs.h"



using namespace std;

namespace Mona {

const UInt32					Thread::MainId(Thread::CurrentId());
thread_local std::string		Thread::_Name("Main");
thread_local Thread*			Thread::_Me(NULL);

void Thread::SetSystemName(const string& name) {
#if defined(_DEBUG)
#if defined(_WIN32)
	typedef struct tagTHREADNAME_INFO {
		DWORD dwType; // Must be 0x1000.
		LPCSTR szName; // Pointer to name (in user addr space).
		DWORD dwThreadID; // Thread ID (-1=caller thread).
		DWORD dwFlags; // Reserved for future use, must be zero.
	} THREADNAME_INFO;

	THREADNAME_INFO info;
	info.dwType = 0x1000;
	info.szName = name.c_str();
	info.dwThreadID = GetCurrentThreadId();
	info.dwFlags = 0;

	__try {
		RaiseException(0x406D1388, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR*)&info);
	} __except (EXCEPTION_EXECUTE_HANDLER) {
	}
#elif defined(__APPLE__)
    pthread_setname_np(name.c_str());
#elif defined(_OS_BSD)
	pthread_set_name_np(pthread_self(), name.c_str());
#else
	prctl(PR_SET_NAME, name.c_str(), 0, 0, 0);
#endif
#endif
}


UInt32 Thread::CurrentId() {
#ifdef _WIN32
	return (UInt32)GetCurrentThreadId();
#elif _OS_BSD
	return (UInt32)syscall(SYS_thread_selfid);
#else
	return (UInt32)syscall(SYS_gettid);
#endif
}

Thread::Thread(const char* name) : _priority(PRIORITY_NORMAL), _stop(true), _name(name) {
}

Thread::~Thread() {
	if (!_stop)
		CRITIC("Thread ",_name," deleting without be stopped before by child class");
	if (_thread.joinable())
		_thread.join();
}

void Thread::process() {
	_Me = this;
	SetSystemName(_Name = _name);

#if !defined(_DEBUG)
	try {
#endif

	// set priority
#if defined(_WIN32)
		static int Priorities[] = { THREAD_PRIORITY_LOWEST, THREAD_PRIORITY_BELOW_NORMAL, THREAD_PRIORITY_NORMAL, THREAD_PRIORITY_ABOVE_NORMAL, THREAD_PRIORITY_HIGHEST };
		if (_priority != PRIORITY_NORMAL && SetThreadPriority(GetCurrentThread(), Priorities[_priority]) == 0)
			WARN("Impossible to change ", _name, " thread priority to ", Priorities[_priority]);
#else
		static int Min = sched_get_priority_min(SCHED_OTHER);
		if(Min==-1) {
			WARN("Impossible to compute minimum ", _name, " thread priority, ",strerror(errno));
		} else {
			static int Max = sched_get_priority_max(SCHED_OTHER);
			if(Max==-1) {
				WARN("Impossible to compute maximum ", _name, " thread priority, ",strerror(errno));
			} else {
				static int Priorities[] = {Min,Min + (Max - Min) / 4,Min + (Max - Min) / 2,Min + (Max - Min) / 4,Max};

				struct sched_param params;
				params.sched_priority = Priorities[_priority];
				int result;
				if ((result=pthread_setschedparam(pthread_self(), SCHED_OTHER , &params)))
					WARN("Impossible to change ", _name, " thread priority to ", Priorities[_priority]," ",strerror(result));
			}
		}
#endif

		Exception ex;
		AUTO_ERROR(run(ex, _stopping), _name);
#if !defined(_DEBUG)
	} catch (exception& ex) {
		CRITIC(_name, ", ", ex.what());
	} catch (...) {
		CRITIC(_name, ", error unknown");
	}
#endif

	_Me = NULL;
	_stop = true;
}


bool Thread::start(Exception& ex, Priority priority) {
	if (!_stop || _Me==this)
		return true;
	std::lock_guard<std::mutex> lock(_mutex);
	if (_thread.joinable())
		_thread.join();
	try {
		_priority = priority;
		wakeUp.reset();
		_stop = false;
		_stopping = false;
		_thread = thread(&Thread::process, this); // start the thread
	} catch (exception& exc) {
		_stop = true;
		ex.set<Ex::System::Thread>("Impossible to start ", _name, " thread, ", exc.what());
		return false;
	}
	return true;
}

void Thread::stop() {
	_stopping = true; // advise thread (intern)
	wakeUp.set(); // wakeUp a sleeping thread
	if (_Me == this) {
		// In the unique case were the caller is the thread itself, set _stop to true to allow to an extern caller to restart it!
		_stop = true;
		return;
	}
	std::lock_guard<std::mutex> lock(_mutex);
	if (_thread.joinable())
		_thread.join();
}


} // namespace Mona
