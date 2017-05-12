/* Copyright 2013-2017 Sathya Laufer
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

#ifndef INODE_H_
#define INODE_H_

#include <atomic>
#include <string>

namespace BaseLib
{

class SharedObjects;

namespace Flows
{

enum class NodeType
{
	binary = 0,
	php = 1
};

class INode
{
public:
	INode(BaseLib::SharedObjects* bl, NodeType type);
	virtual ~INode();
	virtual void dispose();

	virtual NodeType getType() { return _type; }

	void lock();
	void unlock();
	bool locked();
protected:
	BaseLib::SharedObjects* _bl = nullptr;
private:
	NodeType _type;
	std::atomic_bool _locked;

	INode(const INode&) = delete;
	INode& operator=(const INode&) = delete;
};

}
}
#endif