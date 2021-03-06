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

#ifndef SSDP_H_
#define SSDP_H_

#include "../Encoding/Http.h"
#include "../Variable.h"

#include <string>
#include <vector>
#include <memory>
#include <set>

namespace BaseLib
{

class SharedObjects;
class FileDescriptor;

class SsdpInfo
{
public:
	SsdpInfo(std::string ip, PVariable info);
	virtual ~SsdpInfo();
	std::string ip();
	const PVariable info();
private:
	std::string _ip;
	PVariable _info;
};

class Ssdp
{
public:
	Ssdp(BaseLib::SharedObjects* baseLib);
	virtual ~Ssdp();

	/**
	 * Searches for SSDP devices and returns the IPv4 addresses.
	 *
	 * @param[in] stHeader The ST header with the URN to search for (e. g. urn:schemas-upnp-org:device:basic:1)
	 * @param[in] timeout The time to wait for responses
	 * @param[out] devices The found devices with device information parsed from XML to a Homegear variable struct.
	 */
	void searchDevices(const std::string& stHeader, uint32_t timeout, std::vector<SsdpInfo>& devices);
private:
	BaseLib::SharedObjects* _bl = nullptr;
	std::string _address;
	int32_t _port = 1900;

	void getAddress();
	void sendSearchBroadcast(std::shared_ptr<FileDescriptor>& serverSocketDescriptor, const std::string& stHeader, uint32_t timeout);
	void processPacket(Http& http, const std::string& stHeader, std::set<std::string>& locations);
	void getDeviceInfo(std::set<std::string>& locations, std::vector<SsdpInfo>& devices);
	std::shared_ptr<FileDescriptor> getSocketDescriptor();
};

}
#endif
