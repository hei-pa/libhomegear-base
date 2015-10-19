/* Copyright 2013-2015 Sathya Laufer
 *
 * libhomegear-base is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 * 
 * libhomegear-base is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with libhomegear-base.  If not, see
 * <http://www.gnu.org/licenses/>.
 * 
 * In addition, as a special exception, the copyright holders give
 * permission to link the code of portions of this program with the
 * OpenSSL library under certain conditions as described in each
 * individual source file, and distribute linked combinations
 * including the two.
 * You must obey the GNU Lesser General Public License in all respects
 * for all of the code used other than OpenSSL.  If you modify
 * file(s) with this exception, you may extend this exception to your
 * version of the file(s), but you are not obligated to do so.  If you
 * do not wish to do so, delete this exception statement from your
 * version.  If you delete this exception statement from all source
 * files in the program, then also delete it here.
*/

#ifndef IQUEUE_H_
#define IQUEUE_H_

#include <memory>
#include <condition_variable>
#include <thread>

namespace BaseLib
{
class Obj;

class IQueueEntry
{
public:
	IQueueEntry() {};
	virtual ~IQueueEntry() {};
};

class IQueue
{
public:
	IQueue(Obj* baseLib);
	virtual ~IQueue();
	void startQueue(int32_t index, int32_t threadPriority, int32_t threadPolicy);
	void stopQueue(int32_t index);
	bool enqueue(int32_t index, std::shared_ptr<IQueueEntry>& entry);
	virtual void processQueueEntry(int32_t index, std::shared_ptr<IQueueEntry>& entry) = 0;
private:
	Obj* _bl = nullptr;
	static const int32_t _queueCount = 2;
	static const int32_t _bufferSize = 1000;
	std::mutex _bufferMutex[2];
	int32_t _bufferHead[_queueCount];
	int32_t _bufferTail[_queueCount];
	std::shared_ptr<IQueueEntry> _buffer[_queueCount][_bufferSize];
	std::mutex _processingThreadMutex[_queueCount];
	std::thread _processingThread[_queueCount];
	bool _processingEntryAvailable[_queueCount];
	std::condition_variable _processingConditionVariable[_queueCount];
	bool _stopProcessingThread[_queueCount];

	void process(int32_t index);
};

}
#endif
