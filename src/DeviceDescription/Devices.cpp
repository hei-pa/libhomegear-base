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

#include "Devices.h"
#include "../BaseLib.h"
#include "HomeMatic/HmConverter.h"

namespace BaseLib
{
namespace DeviceDescription
{

Devices::Devices(BaseLib::Obj* baseLib, IDevicesEventSink* eventHandler, int32_t family)
{
	_bl = baseLib;
	setEventHandler(eventHandler);
	_family = family;
}

void Devices::clear()
{
	for(std::vector<std::shared_ptr<HomegearDevice>>::iterator i = _devices.begin(); i != _devices.end(); ++i)
	{
		(*i).reset();
	}
	_devices.clear();
}

void Devices::load()
{
	try
	{
		std::string path = _bl->settings.deviceDescriptionPath() + std::to_string((int32_t)_family);
		_devices.clear();
		std::string deviceDir(path);
		std::vector<std::string> files;
		try
		{
			files = _bl->io.getFiles(deviceDir);
		}
		catch(const Exception& ex)
		{
			_bl->out.printError("Could not read device description files in directory: \"" + path + "\": " + ex.what());
			return;
		}
		if(files.empty())
		{
			_bl->out.printError("No xml files found in \"" + path + "\".");
			return;
		}
		for(std::vector<std::string>::iterator i = files.begin(); i != files.end(); ++i)
		{
			std::string filename(deviceDir + "/" + *i);
			std::shared_ptr<HomegearDevice> device = load(filename);
			if(device) _devices.push_back(device);
		}

		if(_devices.empty()) _bl->out.printError("Could not load any devices from xml files in \"" + deviceDir + "\".");
	}
    catch(const std::exception& ex)
    {
    	_bl->out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(const Exception& ex)
    {
    	_bl->out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	_bl->out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

std::shared_ptr<HomegearDevice> Devices::load(std::string& filepath)
{
	try
	{
		if(!Io::fileExists(filepath))
		{
			_bl->out.printError("Error: Could not load device description file \"" + filepath + "\": File does not exist.");
			return std::shared_ptr<HomegearDevice>();
		}
		if(filepath.size() < 5) return std::shared_ptr<HomegearDevice>();
		std::string extension = filepath.substr(filepath.size() - 4, 4);
		HelperFunctions::toLower(extension);
		if(extension != ".xml" && extension != ".hgd") return std::shared_ptr<HomegearDevice>();
		_bl->out.printDebug("Loading XML RPC device " + filepath);
		bool oldFormat = false;
		std::shared_ptr<HomegearDevice> device;
		if(extension == ".hgd")
		{
			std::vector<char> data = Io::getBinaryFileContent(filepath);
			int32_t pos = -1;
			for(uint32_t i = 0; i < 11 && i < data.size(); i++)
			{
				if(data[i] == ' ')
				{
					pos = (int32_t)i;
					break;
				}
			}
			if(pos == -1)
			{
				_bl->out.printError("Error: License module id is missing in encrypted device description file \"" + filepath + "\"");
				return device;
			}
			std::string moduleIdString(&data.at(0), pos);
			int32_t moduleId = BaseLib::Math::getNumber(moduleIdString);
			std::vector<char> input(&data.at(pos + 1), &data.at(data.size() - 1) + 1);
			std::vector<char> xml;
			if(input.empty()) return device;
			if(_eventHandler) ((IDevicesEventSink*)_eventHandler)->onDecryptDeviceDescription(moduleId, input, xml);
			if(!xml.empty()) device.reset(new HomegearDevice(_bl, _family, xml));
		}
		else device.reset(new HomegearDevice(_bl, _family, filepath, oldFormat));
		if(oldFormat) return loadHomeMatic(filepath);
		else if(device && device->loaded()) return device;
	}
    catch(const std::exception& ex)
    {
    	_bl->out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(const Exception& ex)
    {
    	_bl->out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	_bl->out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return std::shared_ptr<HomegearDevice>();
}

std::shared_ptr<HomegearDevice> Devices::loadHomeMatic(std::string& filepath)
{
	try
	{
		if(filepath.empty()) return std::shared_ptr<HomegearDevice>();
		std::string filename = (filepath.find('/') == std::string::npos) ? filepath : filepath.substr(filepath.find_last_of('/') + 1);
		if(filename == "rf_cmm.xml" || filename == "hmw_central.xml" || filename == "hmw_generic.xml" || filename == "rf_central.xml")
		{
			_bl->out.printInfo("Info: Skipping file " + filename + ": File is not needed.");
			return std::shared_ptr<HomegearDevice>();
		}

		std::shared_ptr<HmDeviceDescription::Device> homeMaticDevice(new HmDeviceDescription::Device(_bl, _family, filepath));
		if(!homeMaticDevice || !homeMaticDevice->loaded()) return std::shared_ptr<HomegearDevice>();

		if(filename.compare(0, 3, "rf_") == 0)
		{
			_bl->out.printDebug("Debug: Adding parameters ROAMING and variable CENTRAL_ADDRESS_SPOOFED.");
			std::map<uint32_t, std::shared_ptr<HmDeviceDescription::DeviceChannel>>::iterator channelIterator = homeMaticDevice->channels.find(0);
			if(channelIterator != homeMaticDevice->channels.end() && channelIterator->second)
			{
				std::map<HmDeviceDescription::ParameterSet::Type::Enum, std::shared_ptr<HmDeviceDescription::ParameterSet>>::iterator paramsetIterator = channelIterator->second->parameterSets.find(HmDeviceDescription::ParameterSet::Type::Enum::master);
				if(paramsetIterator != channelIterator->second->parameterSets.end() && paramsetIterator->second)
				{
					if(!paramsetIterator->second->getParameter("ROAMING"))
					{
						std::shared_ptr<HmDeviceDescription::HomeMaticParameter> parameter(new HmDeviceDescription::HomeMaticParameter(_bl));
						parameter->id = "ROAMING";
						parameter->uiFlags = (HmDeviceDescription::HomeMaticParameter::UIFlags::Enum)(HmDeviceDescription::HomeMaticParameter::UIFlags::Enum::visible | HmDeviceDescription::HomeMaticParameter::UIFlags::Enum::internal);
						parameter->logicalParameter.reset(new HmDeviceDescription::LogicalParameterBoolean(_bl));
						parameter->logicalParameter->type = HmDeviceDescription::LogicalParameter::Type::Enum::typeBoolean;
						parameter->logicalParameter->defaultValueExists = true;
						parameter->physicalParameter->interface = HmDeviceDescription::PhysicalParameter::Interface::Enum::store;
						parameter->physicalParameter->type = HmDeviceDescription::PhysicalParameter::Type::Enum::typeInteger;
						paramsetIterator->second->parameters.push_back(parameter);
					}
				}

				paramsetIterator = channelIterator->second->parameterSets.find(HmDeviceDescription::ParameterSet::Type::Enum::values);
				if(paramsetIterator != channelIterator->second->parameterSets.end() && paramsetIterator->second)
				{
					if((homeMaticDevice->rxModes & HmDeviceDescription::Device::RXModes::Enum::always) || (homeMaticDevice->rxModes & HmDeviceDescription::Device::RXModes::Enum::burst))
					{
						_bl->out.printDebug("Debug: Adding parameters POLLING and POLLING_INTERVAL.");
						if(!paramsetIterator->second->getParameter("POLLING"))
						{
							std::shared_ptr<HmDeviceDescription::HomeMaticParameter> parameter(new HmDeviceDescription::HomeMaticParameter(_bl));
							parameter->id = "POLLING";
							parameter->uiFlags = HmDeviceDescription::HomeMaticParameter::UIFlags::Enum::internal;
							parameter->logicalParameter.reset(new HmDeviceDescription::LogicalParameterBoolean(_bl));
							parameter->logicalParameter->type = HmDeviceDescription::LogicalParameter::Type::Enum::typeBoolean;
							parameter->logicalParameter->defaultValueExists = true;
							parameter->physicalParameter->interface = HmDeviceDescription::PhysicalParameter::Interface::Enum::store;
							parameter->physicalParameter->type = HmDeviceDescription::PhysicalParameter::Type::Enum::typeInteger;
							paramsetIterator->second->parameters.push_back(parameter);
						}

						if(!paramsetIterator->second->getParameter("POLLING_INTERVAL"))
						{
							std::shared_ptr<HmDeviceDescription::HomeMaticParameter> parameter(new HmDeviceDescription::HomeMaticParameter(_bl));
							parameter->id = "POLLING_INTERVAL";
							parameter->uiFlags = HmDeviceDescription::HomeMaticParameter::UIFlags::Enum::internal;
							parameter->logicalParameter.reset(new HmDeviceDescription::LogicalParameterInteger(_bl));
							HmDeviceDescription::LogicalParameterInteger* logical = (HmDeviceDescription::LogicalParameterInteger*)parameter->logicalParameter.get();
							logical->type = HmDeviceDescription::LogicalParameter::Type::Enum::typeInteger;
							logical->min = (homeMaticDevice->rxModes & HmDeviceDescription::Device::RXModes::Enum::burst) ? 360 : 10;
							logical->max = 1440;
							logical->defaultValue = (homeMaticDevice->rxModes & HmDeviceDescription::Device::RXModes::Enum::burst) ? 360 : 60;
							logical->defaultValueExists = true;
							logical->unit = "min";
							parameter->physicalParameter->interface = HmDeviceDescription::PhysicalParameter::Interface::Enum::store;
							parameter->physicalParameter->type = HmDeviceDescription::PhysicalParameter::Type::Enum::typeInteger;
							paramsetIterator->second->parameters.push_back(parameter);
						}
					}

					if(!paramsetIterator->second->getParameter("CENTRAL_ADDRESS_SPOOFED"))
					{
						std::shared_ptr<HmDeviceDescription::HomeMaticParameter> parameter(new HmDeviceDescription::HomeMaticParameter(_bl));
						parameter->id = "CENTRAL_ADDRESS_SPOOFED";
						parameter->uiFlags = (HmDeviceDescription::HomeMaticParameter::UIFlags::Enum)(HmDeviceDescription::HomeMaticParameter::UIFlags::Enum::visible | HmDeviceDescription::HomeMaticParameter::UIFlags::Enum::service | HmDeviceDescription::HomeMaticParameter::UIFlags::Enum::sticky);
						parameter->operations = (HmDeviceDescription::HomeMaticParameter::Operations::Enum)(HmDeviceDescription::HomeMaticParameter::Operations::Enum::read | HmDeviceDescription::HomeMaticParameter::Operations::Enum::write | HmDeviceDescription::HomeMaticParameter::Operations::Enum::event);
						parameter->control = "NONE";
						parameter->logicalParameter.reset(new HmDeviceDescription::LogicalParameterEnum(_bl));
						parameter->logicalParameter->type = HmDeviceDescription::LogicalParameter::Type::Enum::typeEnum;
						HmDeviceDescription::LogicalParameterEnum* logical = (HmDeviceDescription::LogicalParameterEnum*)parameter->logicalParameter.get();
						logical->defaultValueExists = true;
						HmDeviceDescription::ParameterOption option;
						option.id = "UNSET";
						option.index = 0;
						option.isDefault = true;
						logical->options.push_back(option);
						option = HmDeviceDescription::ParameterOption();
						option.id = "CENTRAL_ADDRESS_SPOOFED";
						option.index = 1;
						logical->options.push_back(option);
						parameter->physicalParameter->interface = HmDeviceDescription::PhysicalParameter::Interface::Enum::internal;
						parameter->physicalParameter->type = HmDeviceDescription::PhysicalParameter::Type::Enum::typeInteger;
						parameter->physicalParameter->valueID = "CENTRAL_ADDRESS_SPOOFED";
						paramsetIterator->second->parameters.push_back(parameter);
					}
				}
			}
		}
		if(filename == "rf_4dis.xml")
		{
			std::map<std::string, std::shared_ptr<HmDeviceDescription::DeviceFrame>>::iterator frameIterator = homeMaticDevice->framesByID.find("KEY_EVENT_SHORT");
			if(frameIterator != homeMaticDevice->framesByID.end() && frameIterator->second)
			{
				HmDeviceDescription::HomeMaticParameter parameter(_bl);
				parameter.type = HmDeviceDescription::PhysicalParameter::Type::Enum::typeInteger;
				parameter.index = 1.2;
				parameter.size = 0.1;
				parameter.constValue = 1;
				frameIterator->second->parameters.push_front(parameter);
			}

			frameIterator = homeMaticDevice->framesByID.find("KEY_EVENT_LONG");
			if(frameIterator != homeMaticDevice->framesByID.end() && frameIterator->second)
			{
				HmDeviceDescription::HomeMaticParameter parameter(_bl);
				parameter.type = HmDeviceDescription::PhysicalParameter::Type::Enum::typeInteger;
				parameter.index = 1.2;
				parameter.size = 0.1;
				parameter.constValue = 1;
				frameIterator->second->parameters.push_front(parameter);
			}

			frameIterator = homeMaticDevice->framesByID.find("KEY_EVENT_LONG_BIDI");
			if(frameIterator != homeMaticDevice->framesByID.end() && frameIterator->second)
			{
				for(std::list<HmDeviceDescription::HomeMaticParameter>::iterator i = frameIterator->second->parameters.begin(); i != frameIterator->second->parameters.end(); ++i)
				{
					if(i->index == 1.5)
					{
						i->index = 1.2;
						i->constValue = 0;
						break;
					}
				}
			}
		}
		else if(filename == "rf_sec_sd.xml" || filename == "rf_sec_sd_schueco.xml")
		{
			_bl->out.printInfo("Info: Please ignore the warning that \"typeID\" is \"-1\".");

			if(homeMaticDevice->team)
			{
				std::map<uint32_t, std::shared_ptr<HmDeviceDescription::DeviceChannel>>::iterator channelIterator = homeMaticDevice->channels.find(0);
				if(channelIterator != homeMaticDevice->channels.end())
				{
					std::map<HmDeviceDescription::ParameterSet::Type::Enum, std::shared_ptr<HmDeviceDescription::ParameterSet>>::iterator paramsetIterator = channelIterator->second->parameterSets.find(HmDeviceDescription::ParameterSet::Type::Enum::values);
					if(paramsetIterator != channelIterator->second->parameterSets.end())
					{
						std::shared_ptr<HmDeviceDescription::HomeMaticParameter> parameter = paramsetIterator->second->getParameter("LOWBAT");
						if(parameter)
						{
							std::shared_ptr<HmDeviceDescription::PhysicalParameterEvent> eventFrame(new HmDeviceDescription::PhysicalParameterEvent());
							eventFrame->frame = "INFO_LEVEL_ERROR";
							parameter->physicalParameter->eventFrames.push_back(eventFrame);
						}
					}
				}

				std::map<std::string, std::shared_ptr<HmDeviceDescription::DeviceFrame>>::iterator frameIterator = homeMaticDevice->framesByID.find("INFO_LEVEL");
				if(frameIterator != homeMaticDevice->framesByID.end())
				{
					frameIterator->second->subtypeFieldSize = 0.6;
				}

				frameIterator = homeMaticDevice->framesByID.find("INFO_LEVEL_ERROR");
				if(frameIterator == homeMaticDevice->framesByID.end())
				{
					std::shared_ptr<HmDeviceDescription::DeviceFrame> frame(new HmDeviceDescription::DeviceFrame(_bl));
					frame->id = "INFO_LEVEL_ERROR";
					frame->direction = HmDeviceDescription::DeviceFrame::Direction::Enum::fromDevice;
					frame->allowedReceivers = (HmDeviceDescription::DeviceFrame::AllowedReceivers::Enum)(HmDeviceDescription::DeviceFrame::AllowedReceivers::Enum::broadcast | HmDeviceDescription::DeviceFrame::AllowedReceivers::Enum::central | HmDeviceDescription::DeviceFrame::AllowedReceivers::Enum::other);
					frame->isEvent = true;
					frame->type = 0x40;
					frame->fixedChannel = 0;
					HmDeviceDescription::HomeMaticParameter parameter(_bl);
					parameter.type = HmDeviceDescription::PhysicalParameter::Type::Enum::typeInteger;
					parameter.index = 9.7;
					parameter.size = 0.1;
					parameter.param = "LOWBAT";
					frame->parameters.push_back(parameter);
					homeMaticDevice->framesByID["INFO_LEVEL_ERROR"] = frame;
				}

				channelIterator = homeMaticDevice->team->channels.find(1);
				if(channelIterator != homeMaticDevice->team->channels.end())
				{
					std::map<HmDeviceDescription::ParameterSet::Type::Enum, std::shared_ptr<HmDeviceDescription::ParameterSet>>::iterator paramsetIterator = channelIterator->second->parameterSets.find(HmDeviceDescription::ParameterSet::Type::Enum::values);
					if(paramsetIterator != channelIterator->second->parameterSets.end())
					{
						std::shared_ptr<HmDeviceDescription::HomeMaticParameter> parameter = paramsetIterator->second->getParameter("STATE");
						if(parameter) parameter->operations = (HmDeviceDescription::HomeMaticParameter::Operations::Enum)(parameter->operations | HmDeviceDescription::HomeMaticParameter::Operations::Enum::write);

						parameter = paramsetIterator->second->getParameter("INSTALL_TEST");
						if(parameter) parameter->operations = (HmDeviceDescription::HomeMaticParameter::Operations::Enum)(parameter->operations | HmDeviceDescription::HomeMaticParameter::Operations::Enum::write);

						if(!paramsetIterator->second->getParameter("SENDERADDRESS"))
						{
							parameter.reset(new HmDeviceDescription::HomeMaticParameter(_bl));
							parameter->id = "SENDERADDRESS";
							parameter->operations = (HmDeviceDescription::HomeMaticParameter::Operations::Enum)(HmDeviceDescription::HomeMaticParameter::Operations::Enum::read | HmDeviceDescription::HomeMaticParameter::Operations::Enum::event);
							parameter->logicalParameter.reset(new HmDeviceDescription::LogicalParameterString(_bl));
							parameter->logicalParameter->type = HmDeviceDescription::LogicalParameter::Type::Enum::typeString;
							parameter->physicalParameter->interface = HmDeviceDescription::PhysicalParameter::Interface::Enum::command;
							parameter->physicalParameter->type = HmDeviceDescription::PhysicalParameter::Type::Enum::typeString;
							paramsetIterator->second->parameters.push_back(parameter);
						}
					}
				}
			}
		}
		else if(filename == "rf_sen_mdir.xml" || filename == "rf_sen_mdir_v1_5.xml" || filename == "rf_sen_mdir_wm55.xml" || filename == "rf_sec_mdir.xml" || filename == "rf_sec_mdir_v1_5.xml" || filename == "rf_sen_mdir_wm55.xml")
		{
			std::map<uint32_t, std::shared_ptr<HmDeviceDescription::DeviceChannel>>::iterator channelIterator = homeMaticDevice->channels.find(1);
			if(channelIterator != homeMaticDevice->channels.end())
			{
				std::map<HmDeviceDescription::ParameterSet::Type::Enum, std::shared_ptr<HmDeviceDescription::ParameterSet>>::iterator paramsetIterator = channelIterator->second->parameterSets.find(HmDeviceDescription::ParameterSet::Type::Enum::values);
				if(paramsetIterator != channelIterator->second->parameterSets.end())
				{
					std::shared_ptr<HmDeviceDescription::HomeMaticParameter> parameter = paramsetIterator->second->getParameter("NEXT_TRANSMISSION");
					if(parameter)
					{
						parameter->hidden = false;
						parameter->operations = (HmDeviceDescription::HomeMaticParameter::Operations::Enum)(HmDeviceDescription::HomeMaticParameter::Operations::Enum::read | HmDeviceDescription::HomeMaticParameter::Operations::Enum::event);
					}
				}
			}
		}
		else if(filename == "rf_tc_it_wm-w-eu.xml")
		{
			homeMaticDevice->rxModes = (HmDeviceDescription::Device::RXModes::Enum)(homeMaticDevice->rxModes | HmDeviceDescription::Device::RXModes::Enum::wakeUp);

			std::map<std::string, std::shared_ptr<HmDeviceDescription::DeviceFrame>>::iterator frameIterator = homeMaticDevice->framesByID.find("SET_TEMPERATURE_INFO");
			if(frameIterator == homeMaticDevice->framesByID.end())
			{
				std::shared_ptr<HmDeviceDescription::DeviceFrame> frame(new HmDeviceDescription::DeviceFrame(_bl));
				frame->id = "SET_TEMPERATURE_INFO";
				frame->direction = HmDeviceDescription::DeviceFrame::Direction::Enum::fromDevice;
				frame->allowedReceivers = (HmDeviceDescription::DeviceFrame::AllowedReceivers::Enum)(HmDeviceDescription::DeviceFrame::AllowedReceivers::Enum::broadcast | HmDeviceDescription::DeviceFrame::AllowedReceivers::Enum::central | HmDeviceDescription::DeviceFrame::AllowedReceivers::Enum::other);
				frame->isEvent = true;
				frame->type = 0x59;
				frame->fixedChannel = 2;
				HmDeviceDescription::HomeMaticParameter parameter(_bl);
				parameter.type = HmDeviceDescription::PhysicalParameter::Type::Enum::typeInteger;
				parameter.index = 9.0;
				parameter.size = 0.2;
				parameter.param = "CONTROL_MODE";
				frame->parameters.push_back(parameter);
				parameter = HmDeviceDescription::HomeMaticParameter(_bl);
				parameter.type = HmDeviceDescription::PhysicalParameter::Type::Enum::typeInteger;
				parameter.index = 9.2;
				parameter.size = 0.6;
				parameter.param = "SET_TEMPERATURE";
				frame->parameters.push_back(parameter);
				homeMaticDevice->framesByID["SET_TEMPERATURE_INFO"] = frame;
			}
			frameIterator = homeMaticDevice->framesByID.find("WINDOW_STATE_SET");
			if(frameIterator == homeMaticDevice->framesByID.end())
			{
				std::shared_ptr<HmDeviceDescription::DeviceFrame> frame(new HmDeviceDescription::DeviceFrame(_bl));
				frame->id = "WINDOW_STATE_SET";
				frame->direction = HmDeviceDescription::DeviceFrame::Direction::Enum::toDevice;
				frame->type = 0x41;
				frame->subtype = 0x01;
				frame->subtypeIndex = 9;
				HmDeviceDescription::HomeMaticParameter parameter(_bl);
				parameter.type = HmDeviceDescription::PhysicalParameter::Type::Enum::typeInteger;
				parameter.index = 10.0;
				parameter.size = 1.0;
				parameter.constValue = 1;
				frame->parameters.push_back(parameter);
				parameter = HmDeviceDescription::HomeMaticParameter(_bl);
				parameter.type = HmDeviceDescription::PhysicalParameter::Type::Enum::typeInteger;
				parameter.index = 11.0;
				parameter.size = 1.0;
				parameter.param = "WINDOW_STATE";
				frame->parameters.push_back(parameter);
				homeMaticDevice->framesByID["WINDOW_STATE_SET"] = frame;
			}

			std::map<uint32_t, std::shared_ptr<HmDeviceDescription::DeviceChannel>>::iterator channelIterator = homeMaticDevice->channels.find(3);
			if(channelIterator != homeMaticDevice->channels.end() && channelIterator->second)
			{
				std::map<HmDeviceDescription::ParameterSet::Type::Enum, std::shared_ptr<HmDeviceDescription::ParameterSet>>::iterator paramsetIterator = channelIterator->second->parameterSets.find(HmDeviceDescription::ParameterSet::Type::Enum::values);
				if(paramsetIterator != channelIterator->second->parameterSets.end() && paramsetIterator->second)
				{
					if(!paramsetIterator->second->getParameter("WINDOW_STATE"))
					{
						std::shared_ptr<HmDeviceDescription::HomeMaticParameter> parameter(new HmDeviceDescription::HomeMaticParameter(_bl));
						parameter->id = "WINDOW_STATE";
						parameter->control = "SWITCH.STATE";
						parameter->logicalParameter.reset(new HmDeviceDescription::LogicalParameterBoolean(_bl));
						parameter->logicalParameter->type = HmDeviceDescription::LogicalParameter::Type::Enum::typeBoolean;
						parameter->logicalParameter->defaultValueExists = true;
						parameter->physicalParameter->interface = HmDeviceDescription::PhysicalParameter::Interface::Enum::command;
						parameter->physicalParameter->type = HmDeviceDescription::PhysicalParameter::Type::Enum::typeInteger;
						parameter->physicalParameter->valueID = "WINDOW_STATE";
						parameter->physicalParameter->setRequest = "WINDOW_STATE_SET";
						std::shared_ptr<HmDeviceDescription::ParameterConversion> conversion(new HmDeviceDescription::ParameterConversion(_bl, parameter.get()));
						conversion->type = HmDeviceDescription::ParameterConversion::Type::Enum::booleanInteger;
						conversion->threshold = 1;
						conversion->valueFalse = 0;
						conversion->valueTrue = 200;
						parameter->conversion.push_back(conversion);
						paramsetIterator->second->parameters.push_back(parameter);
					}
				}
			}
			channelIterator = homeMaticDevice->channels.find(2);
			if(channelIterator != homeMaticDevice->channels.end())
			{
				std::map<HmDeviceDescription::ParameterSet::Type::Enum, std::shared_ptr<HmDeviceDescription::ParameterSet>>::iterator paramsetIterator = channelIterator->second->parameterSets.find(HmDeviceDescription::ParameterSet::Type::Enum::values);
				if(paramsetIterator != channelIterator->second->parameterSets.end())
				{
					std::shared_ptr<HmDeviceDescription::HomeMaticParameter> parameter = paramsetIterator->second->getParameter("PARTY_MODE_SUBMIT");
					if(parameter) parameter->operations = HmDeviceDescription::HomeMaticParameter::Operations::Enum::write;

					parameter = paramsetIterator->second->getParameter("CONTROL_MODE");
					if(parameter)
					{
						std::shared_ptr<HmDeviceDescription::PhysicalParameterEvent> eventFrame(new HmDeviceDescription::PhysicalParameterEvent());
						eventFrame->frame = "SET_TEMPERATURE_INFO";
						bool frameExists = false;
						for(std::vector<std::shared_ptr<HmDeviceDescription::PhysicalParameterEvent>>::iterator i = parameter->physicalParameter->eventFrames.begin(); i != parameter->physicalParameter->eventFrames.end(); ++i)
						{
							if((*i)->frame == "SET_TEMPERATURE_INFO")
							{
								frameExists = true;
								break;
							}
						}
						if(!frameExists) parameter->physicalParameter->eventFrames.push_back(eventFrame);
					}

					parameter = paramsetIterator->second->getParameter("SET_TEMPERATURE");
					if(parameter)
					{
						std::shared_ptr<HmDeviceDescription::PhysicalParameterEvent> eventFrame(new HmDeviceDescription::PhysicalParameterEvent());
						eventFrame->frame = "SET_TEMPERATURE_INFO";
						bool frameExists = false;
						for(std::vector<std::shared_ptr<HmDeviceDescription::PhysicalParameterEvent>>::iterator i = parameter->physicalParameter->eventFrames.begin(); i != parameter->physicalParameter->eventFrames.end(); ++i)
						{
							if((*i)->frame == "SET_TEMPERATURE_INFO")
							{
								frameExists = true;
								break;
							}
						}
						if(!frameExists) parameter->physicalParameter->eventFrames.push_back(eventFrame);
					}
				}
			}
		}
		else if(filename == "rf_wds30_ot2.xml")
		{
			std::map<std::string, std::shared_ptr<HmDeviceDescription::DeviceFrame>>::iterator frameIterator = homeMaticDevice->framesByID.find("WEATHER_EVENT");
			if(frameIterator != homeMaticDevice->framesByID.end()) frameIterator->second->id = "WEATHER_EVENT5";

			frameIterator = homeMaticDevice->framesByID.find("MEASURE_EVENT");
			while(frameIterator != homeMaticDevice->framesByID.end())
			{
				if(frameIterator->second->channelField == 10)
				{
					frameIterator->second->id = "MEASURE_EVENT1";
					homeMaticDevice->framesByID["MEASURE_EVENT1"] = frameIterator->second;
					homeMaticDevice->framesByID.erase(frameIterator);
				}
				else if(frameIterator->second->channelField == 13)
				{
					frameIterator->second->id = "MEASURE_EVENT2";
					homeMaticDevice->framesByID["MEASURE_EVENT2"] = frameIterator->second;
					homeMaticDevice->framesByID.erase(frameIterator);
				}
				else if(frameIterator->second->channelField == 16)
				{
					frameIterator->second->id = "MEASURE_EVENT3";
					homeMaticDevice->framesByID["MEASURE_EVENT3"] = frameIterator->second;
					homeMaticDevice->framesByID.erase(frameIterator);
				}
				else if(frameIterator->second->channelField == 19)
				{
					frameIterator->second->id = "MEASURE_EVENT4";
					homeMaticDevice->framesByID["MEASURE_EVENT4"] = frameIterator->second;
					homeMaticDevice->framesByID.erase(frameIterator);
				}
				frameIterator = homeMaticDevice->framesByID.find("MEASURE_EVENT");
			}

			std::map<uint32_t, std::shared_ptr<HmDeviceDescription::DeviceChannel>>::iterator channelIterator = homeMaticDevice->channels.find(1);
			if(channelIterator != homeMaticDevice->channels.end())
			{
				std::map<HmDeviceDescription::ParameterSet::Type::Enum, std::shared_ptr<HmDeviceDescription::ParameterSet>>::iterator paramsetIterator = channelIterator->second->parameterSets.find(HmDeviceDescription::ParameterSet::Type::Enum::values);
				if(paramsetIterator != channelIterator->second->parameterSets.end())
				{
					std::shared_ptr<HmDeviceDescription::HomeMaticParameter> parameter = paramsetIterator->second->getParameter("TEMPERATURE");if(parameter)
					{
						for(int32_t i = 1; i <= 4; i++)
						{
							std::shared_ptr<HmDeviceDescription::PhysicalParameterEvent> eventFrame(new HmDeviceDescription::PhysicalParameterEvent());
							eventFrame->frame = "MEASURE_EVENT" + std::to_string(i);
							bool frameExists = false;
							for(std::vector<std::shared_ptr<HmDeviceDescription::PhysicalParameterEvent>>::iterator j = parameter->physicalParameter->eventFrames.begin(); j != parameter->physicalParameter->eventFrames.end(); ++j)
							{
								if(i == 1 && (*j)->frame == "MEASURE_EVENT")
								{
									(*j)->frame = "MEASURE_EVENT1";
								}
								else if((*j)->frame == "MEASURE_EVENT" + std::to_string(i))
								{
									frameExists = true;
									break;
								}
							}
							if(!frameExists) parameter->physicalParameter->eventFrames.push_back(eventFrame);
						}
					}
				}
			}

			channelIterator = homeMaticDevice->channels.find(5);
			if(channelIterator != homeMaticDevice->channels.end())
			{
				std::map<HmDeviceDescription::ParameterSet::Type::Enum, std::shared_ptr<HmDeviceDescription::ParameterSet>>::iterator paramsetIterator = channelIterator->second->parameterSets.find(HmDeviceDescription::ParameterSet::Type::Enum::values);
				if(paramsetIterator != channelIterator->second->parameterSets.end())
				{
					std::shared_ptr<HmDeviceDescription::HomeMaticParameter> parameter = paramsetIterator->second->getParameter("TEMPERATURE");if(parameter)
					{
						for(std::vector<std::shared_ptr<HmDeviceDescription::PhysicalParameterEvent>>::iterator j = parameter->physicalParameter->eventFrames.begin(); j != parameter->physicalParameter->eventFrames.end(); ++j)
						{
							if((*j)->frame == "WEATHER_EVENT")
							{
								(*j)->frame = "WEATHER_EVENT5";
								break;
							}
						}
					}
				}
			}
		}
		else if(filename == "rf_bl.xml" || filename == "rf_bl_644.xml" || filename == "rf_bl_conf_644.xml" || filename == "rf_bl_conf_644_e_v2_0.xml" || filename == "rf_bl_conf_644_e_v2_1.xml" || filename == "rf_bl_le_v2_3.xml")
		{
			std::map<std::string, std::shared_ptr<HmDeviceDescription::DeviceFrame>>::iterator frameIterator = homeMaticDevice->framesByID.find("INSTALL_TEST");
			if(frameIterator != homeMaticDevice->framesByID.end())
			{
				for(std::list<HmDeviceDescription::HomeMaticParameter>::iterator i = frameIterator->second->parameters.begin(); i != frameIterator->second->parameters.end(); ++i)
				{
					if(i->param == "IT_COMMAND")
					{
						i->param = "";
						i->constValue = 2;
						break;
					}
				}
			}
		}
		else if(filename == "rf_cc_rt_dn.xml" || filename == "rf_cc_rt_dn_bom.xml")
		{
			std::map<std::string, std::shared_ptr<HmDeviceDescription::DeviceFrame>>::iterator frameIterator = homeMaticDevice->framesByID.find("WINDOW_STATE_SET");
			if(frameIterator == homeMaticDevice->framesByID.end())
			{
				std::shared_ptr<HmDeviceDescription::DeviceFrame> frame(new HmDeviceDescription::DeviceFrame(_bl));
				frame->id = "WINDOW_STATE_SET";
				frame->direction = HmDeviceDescription::DeviceFrame::Direction::Enum::toDevice;
				frame->type = 0x41;
				frame->subtype = 0x01;
				frame->subtypeIndex = 9;
				HmDeviceDescription::HomeMaticParameter parameter(_bl);
				parameter.type = HmDeviceDescription::PhysicalParameter::Type::Enum::typeInteger;
				parameter.index = 10.0;
				parameter.size = 1.0;
				parameter.constValue = 1;
				frame->parameters.push_back(parameter);
				parameter = HmDeviceDescription::HomeMaticParameter(_bl);
				parameter.type = HmDeviceDescription::PhysicalParameter::Type::Enum::typeInteger;
				parameter.index = 11.0;
				parameter.size = 1.0;
				parameter.param = "WINDOW_STATE";
				frame->parameters.push_back(parameter);
				homeMaticDevice->framesByID["WINDOW_STATE_SET"] = frame;
			}

			std::map<uint32_t, std::shared_ptr<HmDeviceDescription::DeviceChannel>>::iterator channelIterator = homeMaticDevice->channels.find(3);
			if(channelIterator != homeMaticDevice->channels.end() && channelIterator->second)
			{
				std::map<HmDeviceDescription::ParameterSet::Type::Enum, std::shared_ptr<HmDeviceDescription::ParameterSet>>::iterator paramsetIterator = channelIterator->second->parameterSets.find(HmDeviceDescription::ParameterSet::Type::Enum::values);
				if(paramsetIterator != channelIterator->second->parameterSets.end() && paramsetIterator->second)
				{
					if(!paramsetIterator->second->getParameter("WINDOW_STATE"))
					{
						std::shared_ptr<HmDeviceDescription::HomeMaticParameter> parameter(new HmDeviceDescription::HomeMaticParameter(_bl));
						parameter->id = "WINDOW_STATE";
						parameter->control = "SWITCH.STATE";
						parameter->logicalParameter.reset(new HmDeviceDescription::LogicalParameterBoolean(_bl));
						parameter->logicalParameter->type = HmDeviceDescription::LogicalParameter::Type::Enum::typeBoolean;
						parameter->logicalParameter->defaultValueExists = true;
						parameter->physicalParameter->interface = HmDeviceDescription::PhysicalParameter::Interface::Enum::command;
						parameter->physicalParameter->type = HmDeviceDescription::PhysicalParameter::Type::Enum::typeInteger;
						parameter->physicalParameter->valueID = "WINDOW_STATE";
						parameter->physicalParameter->setRequest = "WINDOW_STATE_SET";
						std::shared_ptr<HmDeviceDescription::ParameterConversion> conversion(new HmDeviceDescription::ParameterConversion(_bl, parameter.get()));
						conversion->type = HmDeviceDescription::ParameterConversion::Type::Enum::booleanInteger;
						conversion->threshold = 1;
						conversion->valueFalse = 0;
						conversion->valueTrue = 200;
						parameter->conversion.push_back(conversion);
						paramsetIterator->second->parameters.push_back(parameter);
					}
				}
			}
		}
		else if(filename == "rf_dis_wm55.xml")
		{
			_bl->out.printInfo("Info: Please ignore the warnings regarding \"multiframe_command\" and \"multiframe_command_frame\".");
			homeMaticDevice->rxModes = (HmDeviceDescription::Device::RXModes::Enum)(homeMaticDevice->rxModes | HmDeviceDescription::Device::RXModes::Enum::wakeUp2);

			std::map<std::string, std::shared_ptr<HmDeviceDescription::DeviceFrame>>::iterator frameIterator = homeMaticDevice->framesByID.find("SEND_TEXT");
			if(frameIterator == homeMaticDevice->framesByID.end())
			{
				std::shared_ptr<HmDeviceDescription::DeviceFrame> frame(new HmDeviceDescription::DeviceFrame(_bl));
				frame->id = "SEND_TEXT";
				frame->maxPackets = 8;
				frame->splitAfter = 17;
				frame->direction = HmDeviceDescription::DeviceFrame::Direction::Enum::toDevice;
				frame->type = 0x11;
				frame->subtype = 0x80;
				frame->subtypeIndex = 9;
				frame->channelField = 10;
				HmDeviceDescription::HomeMaticParameter parameter(_bl);
				parameter.type = HmDeviceDescription::PhysicalParameter::Type::Enum::typeString;
				parameter.index = 11.0;
				parameter.size = 15.0;
				parameter.param = "SUBMIT";
				frame->parameters.push_back(parameter);
				homeMaticDevice->framesByID["SEND_TEXT"] = frame;
			}

			std::map<uint32_t, std::shared_ptr<HmDeviceDescription::DeviceChannel>>::iterator channelIterator = homeMaticDevice->channels.find(0);
			if(channelIterator != homeMaticDevice->channels.end()) channelIterator->second->aesAlways = true;

			for(int32_t i = 1; i <= 2; ++i)
			{
				std::map<uint32_t, std::shared_ptr<HmDeviceDescription::DeviceChannel>>::iterator channelIterator = homeMaticDevice->channels.find(i);
				if(channelIterator != homeMaticDevice->channels.end() && channelIterator->second)
				{
					std::map<HmDeviceDescription::ParameterSet::Type::Enum, std::shared_ptr<HmDeviceDescription::ParameterSet>>::iterator paramsetIterator = channelIterator->second->parameterSets.find(HmDeviceDescription::ParameterSet::Type::Enum::values);
					if(paramsetIterator != channelIterator->second->parameterSets.end() && paramsetIterator->second)
					{
						std::shared_ptr<HmDeviceDescription::HomeMaticParameter> parameter = paramsetIterator->second->getParameter("SUBMIT");
						if(parameter)
						{
							parameter->physicalParameter->interface = HmDeviceDescription::PhysicalParameter::Interface::Enum::command;
							parameter->physicalParameter->id = "SUBMIT";
							parameter->physicalParameter->valueID = "SUBMIT";
							parameter->physicalParameter->setRequest = "SEND_TEXT";
							if(parameter->conversion.empty())
							{
								std::shared_ptr<HmDeviceDescription::ParameterConversion> conversion(new HmDeviceDescription::ParameterConversion(_bl, parameter.get()));
								conversion->type = HmDeviceDescription::ParameterConversion::Type::Enum::hexstringBytearray;
								parameter->conversion.push_back(conversion);
							}
						}
					}
				}
			}
		}
		else if(filename == "rf_es_pmsw.xml" || filename == "rf_es_pmsw_le_v2_4.xml")
		{
			std::map<std::string, std::shared_ptr<HmDeviceDescription::DeviceFrame>>::iterator frameIterator = homeMaticDevice->framesByID.find("POWER_EVENT_CYCLIC");
			if(frameIterator != homeMaticDevice->framesByID.end())
			{
				std::list<HmDeviceDescription::HomeMaticParameter> newFrameParameters;
				for(std::list<HmDeviceDescription::HomeMaticParameter>::iterator i = frameIterator->second->parameters.begin(); i != frameIterator->second->parameters.end(); ++i)
				{
					if(i->param != "BOOT") newFrameParameters.push_back(*i);
				}
				frameIterator->second->parameters.clear();
				frameIterator->second->parameters = newFrameParameters;
			}

			std::map<uint32_t, std::shared_ptr<HmDeviceDescription::DeviceChannel>>::iterator channelIterator = homeMaticDevice->channels.find(2);
			if(channelIterator != homeMaticDevice->channels.end() && channelIterator->second)
			{
				std::map<HmDeviceDescription::ParameterSet::Type::Enum, std::shared_ptr<HmDeviceDescription::ParameterSet>>::iterator paramsetIterator = channelIterator->second->parameterSets.find(HmDeviceDescription::ParameterSet::Type::Enum::values);
				if(paramsetIterator != channelIterator->second->parameterSets.end() && paramsetIterator->second)
				{
					std::shared_ptr<HmDeviceDescription::HomeMaticParameter> parameter = paramsetIterator->second->getParameter("BOOT");
					if(parameter)
					{
						std::vector<std::shared_ptr<HmDeviceDescription::PhysicalParameterEvent>> newEventFrames;
						for(std::vector<std::shared_ptr<HmDeviceDescription::PhysicalParameterEvent>>::iterator i = parameter->physicalParameter->eventFrames.begin(); i != parameter->physicalParameter->eventFrames.end(); ++i)
						{
							if((*i)->frame != "POWER_EVENT_CYCLIC") newEventFrames.push_back(*i);
						}
						parameter->physicalParameter->eventFrames.clear();
						parameter->physicalParameter->eventFrames = newEventFrames;
					}
				}
			}
		}
		else if(filename == "rf_keymatic.xml")
		{
			std::map<std::string, std::shared_ptr<HmDeviceDescription::DeviceFrame>>::iterator frameIterator = homeMaticDevice->framesByID.find("INFO_LEVEL_LOWBAT");
			if(frameIterator == homeMaticDevice->framesByID.end())
			{
				std::shared_ptr<HmDeviceDescription::DeviceFrame> frame(new HmDeviceDescription::DeviceFrame(_bl));
				frame->id = "INFO_LEVEL_LOWBAT";
				frame->direction = HmDeviceDescription::DeviceFrame::Direction::Enum::fromDevice;
				frame->allowedReceivers = HmDeviceDescription::DeviceFrame::AllowedReceivers::Enum::central;
				frame->isEvent = true;
				frame->type = 0x10;
				frame->subtype = 0x06;
				frame->subtypeIndex = 9;
				frame->fixedChannel = 0;
				HmDeviceDescription::HomeMaticParameter parameter(_bl);
				parameter.type = HmDeviceDescription::PhysicalParameter::Type::Enum::typeInteger;
				parameter.index = 12.7;
				parameter.size = 0.1;
				parameter.param = "LOWBAT";
				frame->parameters.push_back(parameter);
				homeMaticDevice->framesByID["INFO_LEVEL_LOWBAT"] = frame;

				frame.reset(new HmDeviceDescription::DeviceFrame(_bl));
				frame->id = "ACK_STATUS_LOWBAT";
				frame->direction = HmDeviceDescription::DeviceFrame::Direction::Enum::fromDevice;
				frame->allowedReceivers = HmDeviceDescription::DeviceFrame::AllowedReceivers::Enum::central;
				frame->isEvent = true;
				frame->type = 0x02;
				frame->subtype = 0x01;
				frame->subtypeIndex = 9;
				frame->fixedChannel = 0;
				parameter = HmDeviceDescription::HomeMaticParameter(_bl);
				parameter.type = HmDeviceDescription::PhysicalParameter::Type::Enum::typeInteger;
				parameter.index = 12.7;
				parameter.size = 0.1;
				parameter.param = "LOWBAT";
				frame->parameters.push_back(parameter);
				homeMaticDevice->framesByID["ACK_STATUS_LOWBAT"] = frame;
			}

			std::map<uint32_t, std::shared_ptr<HmDeviceDescription::DeviceChannel>>::iterator channelIterator = homeMaticDevice->channels.find(0);
			if(channelIterator != homeMaticDevice->channels.end() && channelIterator->second)
			{
				std::map<HmDeviceDescription::ParameterSet::Type::Enum, std::shared_ptr<HmDeviceDescription::ParameterSet>>::iterator paramsetIterator = channelIterator->second->parameterSets.find(HmDeviceDescription::ParameterSet::Type::Enum::values);
				if(paramsetIterator != channelIterator->second->parameterSets.end() && paramsetIterator->second)
				{
					std::shared_ptr<HmDeviceDescription::HomeMaticParameter> parameter = paramsetIterator->second->getParameter("LOWBAT");
					if(parameter)
					{
						if(parameter->physicalParameter->eventFrames.empty())
						{
							std::shared_ptr<HmDeviceDescription::PhysicalParameterEvent> eventFrame(new HmDeviceDescription::PhysicalParameterEvent());
							eventFrame->frame = "INFO_LEVEL_LOWBAT";
							parameter->physicalParameter->eventFrames.push_back(eventFrame);
							eventFrame.reset(new HmDeviceDescription::PhysicalParameterEvent());
							eventFrame->frame = "ACK_STATUS_LOWBAT";
							parameter->physicalParameter->eventFrames.push_back(eventFrame);
						}
					}
				}
			}
		}
		else if(filename == "rf_roto_wdf_solar.xml")
		{
			_bl->out.printInfo("Info: Please ignore the warnings regarding the unknown node name \"parameter\" for \"paramset_defs\".");
		}
		else if(filename == "rf_rc.xml" || filename == "rf_rc_12.xml" || filename == "rf_rc_19.xml" || filename == "rf_rc-4-2.xml" || filename == "rf_rc-key4-2.xml" || filename == "rf_rc-sec4-2.xml" || filename == "rf_rc-4-3_single_on.xml" || filename == "rf_rc_2_fm.xml" || filename == "rf_rc_dis.xml" || filename == "rf_rc_single_on.xml")
		{
			for(std::map<uint32_t, std::shared_ptr<HmDeviceDescription::DeviceChannel>>::iterator i = homeMaticDevice->channels.begin(); i != homeMaticDevice->channels.end(); ++i)
			{
				if(!i->second) continue;
				std::map<HmDeviceDescription::ParameterSet::Type::Enum, std::shared_ptr<HmDeviceDescription::ParameterSet>>::iterator paramsetIterator = i->second->parameterSets.find(HmDeviceDescription::ParameterSet::Type::Enum::values);
				if(paramsetIterator != i->second->parameterSets.end() && paramsetIterator->second)
				{
					if(i->first == 18 && filename == "rf_rc_19.xml")
					{
						if(!paramsetIterator->second->getParameter("COMMA"))
						{
							std::shared_ptr<HmDeviceDescription::HomeMaticParameter> parameter(new HmDeviceDescription::HomeMaticParameter(_bl));
							parameter->id = "COMMA";
							parameter->operations = HmDeviceDescription::HomeMaticParameter::Operations::Enum::write;
							parameter->logicalParameter.reset(new HmDeviceDescription::LogicalParameterAction(_bl));
							parameter->logicalParameter->type = HmDeviceDescription::LogicalParameter::Type::Enum::typeAction;
							parameter->physicalParameter->interface = HmDeviceDescription::PhysicalParameter::Interface::Enum::store;
							parameter->physicalParameter->type = HmDeviceDescription::PhysicalParameter::Type::Enum::typeInteger;
							parameter->physicalParameter->id = "COMMA";
							paramsetIterator->second->parameters.push_back(parameter);
						}
					}

					if(i->second->type != "KEY" && i->second->type != "CENTRAL_KEY") continue;
					if(!paramsetIterator->second->getParameter("PRESS_SHORT")) continue;
					if(filename == "rf_rc-key4-2.xml" || filename == "rf_rc-sec4-2.xml")
					{
						std::map<std::string, std::shared_ptr<HmDeviceDescription::DeviceFrame>>::iterator frameIterator = homeMaticDevice->framesByID.find("KEY_EVENT_LONG_BIDI");
						if(frameIterator == homeMaticDevice->framesByID.end())
						{
							std::shared_ptr<HmDeviceDescription::DeviceFrame> frame(new HmDeviceDescription::DeviceFrame(_bl));
							frame->id = "KEY_EVENT_LONG_BIDI";
							frame->direction = HmDeviceDescription::DeviceFrame::Direction::Enum::fromDevice;
							frame->allowedReceivers = (HmDeviceDescription::DeviceFrame::AllowedReceivers::Enum)(HmDeviceDescription::DeviceFrame::AllowedReceivers::Enum::central | HmDeviceDescription::DeviceFrame::AllowedReceivers::Enum::broadcast | HmDeviceDescription::DeviceFrame::AllowedReceivers::Enum::other);
							frame->isEvent = true;
							frame->type = 0x40;
							frame->channelField = 9;
							frame->channelFieldSize = 0.6;
							HmDeviceDescription::HomeMaticParameter parameter(_bl);
							parameter.type = HmDeviceDescription::PhysicalParameter::Type::Enum::typeInteger;
							parameter.index = 1.5;
							parameter.size = 0.1;
							parameter.constValue = 1;
							frame->parameters.push_back(parameter);
							parameter = HmDeviceDescription::HomeMaticParameter(_bl);
							parameter.type = HmDeviceDescription::PhysicalParameter::Type::Enum::typeInteger;
							parameter.index = 9.6;
							parameter.size = 0.1;
							parameter.constValue = 1;
							frame->parameters.push_back(parameter);
							parameter = HmDeviceDescription::HomeMaticParameter(_bl);
							parameter.type = HmDeviceDescription::PhysicalParameter::Type::Enum::typeInteger;
							parameter.index = 10.0;
							parameter.size = 1.0;
							parameter.param = "COUNTER";
							frame->parameters.push_back(parameter);
							parameter = HmDeviceDescription::HomeMaticParameter(_bl);
							parameter.type = HmDeviceDescription::PhysicalParameter::Type::Enum::typeInteger;
							parameter.index = 10.0;
							parameter.size = 1.0;
							parameter.param = "TEST_COUNTER";
							frame->parameters.push_back(parameter);
							homeMaticDevice->framesByID["KEY_EVENT_LONG_BIDI"] = frame;
						}

						if(!paramsetIterator->second->getParameter("PRESS_LONG_RELEASE"))
						{
							std::shared_ptr<HmDeviceDescription::HomeMaticParameter> parameter(new HmDeviceDescription::HomeMaticParameter(_bl));
							parameter->id = "PRESS_LONG_RELEASE";
							parameter->operations = HmDeviceDescription::HomeMaticParameter::Operations::Enum::event;
							parameter->uiFlags = HmDeviceDescription::HomeMaticParameter::UIFlags::Enum::internal;
							parameter->logicalParameter.reset(new HmDeviceDescription::LogicalParameterAction(_bl));
							parameter->logicalParameter->type = HmDeviceDescription::LogicalParameter::Type::Enum::typeAction;
							parameter->physicalParameter->interface = HmDeviceDescription::PhysicalParameter::Interface::Enum::command;
							parameter->physicalParameter->type = HmDeviceDescription::PhysicalParameter::Type::Enum::typeInteger;
							parameter->physicalParameter->valueID = "COUNTER";
							std::shared_ptr<HmDeviceDescription::PhysicalParameterEvent> eventFrame(new HmDeviceDescription::PhysicalParameterEvent());
							eventFrame->frame = "KEY_EVENT_LONG_BIDI";
							parameter->physicalParameter->eventFrames.push_back(eventFrame);
							paramsetIterator->second->parameters.push_back(parameter);
						}
						if(!paramsetIterator->second->getParameter("PRESS_CONT"))
						{
							std::shared_ptr<HmDeviceDescription::HomeMaticParameter> parameter(new HmDeviceDescription::HomeMaticParameter(_bl));
							parameter->id = "PRESS_CONT";
							parameter->operations = HmDeviceDescription::HomeMaticParameter::Operations::Enum::event;
							parameter->uiFlags = HmDeviceDescription::HomeMaticParameter::UIFlags::Enum::internal;
							parameter->logicalParameter.reset(new HmDeviceDescription::LogicalParameterAction(_bl));
							parameter->logicalParameter->type = HmDeviceDescription::LogicalParameter::Type::Enum::typeAction;
							parameter->physicalParameter->interface = HmDeviceDescription::PhysicalParameter::Interface::Enum::command;
							parameter->physicalParameter->type = HmDeviceDescription::PhysicalParameter::Type::Enum::typeInteger;
							parameter->physicalParameter->valueID = "COUNTER";
							std::shared_ptr<HmDeviceDescription::PhysicalParameterEvent> eventFrame(new HmDeviceDescription::PhysicalParameterEvent());
							eventFrame->frame = "KEY_EVENT_LONG";
							parameter->physicalParameter->eventFrames.push_back(eventFrame);
							paramsetIterator->second->parameters.push_back(parameter);
						}
					}
					if(!paramsetIterator->second->getParameter("TEST_COUNTER"))
					{
						std::shared_ptr<HmDeviceDescription::HomeMaticParameter> parameter(new HmDeviceDescription::HomeMaticParameter(_bl));
						parameter->id = "TEST_COUNTER";
						parameter->operations = (HmDeviceDescription::HomeMaticParameter::Operations::Enum)(HmDeviceDescription::HomeMaticParameter::Operations::Enum::read | HmDeviceDescription::HomeMaticParameter::Operations::Enum::event);
						parameter->logicalParameter.reset(new HmDeviceDescription::LogicalParameterInteger(_bl));
						parameter->logicalParameter->type = HmDeviceDescription::LogicalParameter::Type::Enum::typeInteger;
						parameter->physicalParameter->interface = HmDeviceDescription::PhysicalParameter::Interface::Enum::command;
						parameter->physicalParameter->type = HmDeviceDescription::PhysicalParameter::Type::Enum::typeInteger;
						parameter->physicalParameter->valueID = "TEST_COUNTER";
						std::shared_ptr<HmDeviceDescription::PhysicalParameterEvent> eventFrame(new HmDeviceDescription::PhysicalParameterEvent());
						eventFrame->frame = "KEY_EVENT_SHORT";
						parameter->physicalParameter->eventFrames.push_back(eventFrame);
						eventFrame.reset(new HmDeviceDescription::PhysicalParameterEvent());
						eventFrame->frame = "KEY_EVENT_LONG";
						parameter->physicalParameter->eventFrames.push_back(eventFrame);
						paramsetIterator->second->parameters.push_back(parameter);
					}
					if(!paramsetIterator->second->getParameter("SIM_COUNTER"))
					{
						std::shared_ptr<HmDeviceDescription::HomeMaticParameter> parameter(new HmDeviceDescription::HomeMaticParameter(_bl));
						parameter->id = "SIM_COUNTER";
						parameter->operations = (HmDeviceDescription::HomeMaticParameter::Operations::Enum)(HmDeviceDescription::HomeMaticParameter::Operations::Enum::read | HmDeviceDescription::HomeMaticParameter::Operations::Enum::event);
						parameter->logicalParameter.reset(new HmDeviceDescription::LogicalParameterInteger(_bl));
						parameter->logicalParameter->type = HmDeviceDescription::LogicalParameter::Type::Enum::typeInteger;
						parameter->physicalParameter->interface = HmDeviceDescription::PhysicalParameter::Interface::Enum::command;
						parameter->physicalParameter->type = HmDeviceDescription::PhysicalParameter::Type::Enum::typeInteger;
						parameter->physicalParameter->valueID = "SIM_COUNTER";
						std::shared_ptr<HmDeviceDescription::PhysicalParameterEvent> eventFrame(new HmDeviceDescription::PhysicalParameterEvent());
						eventFrame->frame = "KEY_SIM_SHORT";
						parameter->physicalParameter->eventFrames.push_back(eventFrame);
						eventFrame.reset(new HmDeviceDescription::PhysicalParameterEvent());
						eventFrame->frame = "KEY_SIM_LONG";
						parameter->physicalParameter->eventFrames.push_back(eventFrame);
						paramsetIterator->second->parameters.push_back(parameter);
					}
				}
			}
		}
		else if(filename == "rf_rhs.xml" || filename == "rf_rhs_e_v1_7.xml" || filename == "rf_rhs_le_v1_6.xml")
		{
			std::map<uint32_t, std::shared_ptr<HmDeviceDescription::DeviceChannel>>::iterator channelIterator = homeMaticDevice->channels.find(1);
			if(channelIterator != homeMaticDevice->channels.end() && channelIterator->second)
			{
				std::map<HmDeviceDescription::ParameterSet::Type::Enum, std::shared_ptr<HmDeviceDescription::ParameterSet>>::iterator paramsetIterator = channelIterator->second->parameterSets.find(HmDeviceDescription::ParameterSet::Type::Enum::master);
				if(paramsetIterator != channelIterator->second->parameterSets.end() && paramsetIterator->second)
				{
					std::shared_ptr<HmDeviceDescription::HomeMaticParameter> aesActive;
					std::vector<std::shared_ptr<HmDeviceDescription::HomeMaticParameter>> newParameters;
					for(std::vector<std::shared_ptr<HmDeviceDescription::HomeMaticParameter>>::iterator i = paramsetIterator->second->parameters.begin(); i != paramsetIterator->second->parameters.end(); ++i)
					{
						if((*i)->id == "AES_ACTIVE") aesActive = *i;
						else newParameters.push_back(*i);
					}
					if(aesActive) newParameters.push_back(aesActive);
					paramsetIterator->second->parameters = newParameters;
				}
			}
		}
		else if(filename == "rf_s.xml" || filename == "rf_s_le_v2_3.xml" || filename == "rf_s_1conf_644.xml" || filename == "rf_s_1conf_644_le_v2_1.xml" || filename == "rf_s_1conf_644_le_v2_3.xml")
		{
			std::map<std::string, std::shared_ptr<HmDeviceDescription::DeviceFrame>>::iterator frameIterator = homeMaticDevice->framesByID.find("INFO_POWERON");
			if(frameIterator != homeMaticDevice->framesByID.end())
			{
				bool parameterExists = false;
				for(std::list<HmDeviceDescription::HomeMaticParameter>::iterator i = frameIterator->second->parameters.begin(); i != frameIterator->second->parameters.end(); ++i)
				{
					if(i->param == "BOOT")
					{
						parameterExists = true;
						break;
					}
				}
				if(!parameterExists)
				{
					HmDeviceDescription::HomeMaticParameter parameter(_bl);
					parameter.type = HmDeviceDescription::PhysicalParameter::Type::Enum::typeInteger;
					parameter.constValue = 1;
					parameter.param = "BOOT";
					frameIterator->second->parameters.push_back(parameter);
				}
			}

			std::map<uint32_t, std::shared_ptr<HmDeviceDescription::DeviceChannel>>::iterator channelIterator = homeMaticDevice->channels.find(0);
			if(channelIterator != homeMaticDevice->channels.end() && channelIterator->second)
			{
				std::map<HmDeviceDescription::ParameterSet::Type::Enum, std::shared_ptr<HmDeviceDescription::ParameterSet>>::iterator paramsetIterator = channelIterator->second->parameterSets.find(HmDeviceDescription::ParameterSet::Type::Enum::values);
				if(paramsetIterator != channelIterator->second->parameterSets.end() && paramsetIterator->second)
				{
					if(!paramsetIterator->second->getParameter("BOOT"))
					{
						std::shared_ptr<HmDeviceDescription::HomeMaticParameter> parameter(new HmDeviceDescription::HomeMaticParameter(_bl));
						parameter->id = "BOOT";
						parameter->operations = (HmDeviceDescription::HomeMaticParameter::Operations::Enum)(HmDeviceDescription::HomeMaticParameter::Operations::Enum::read | HmDeviceDescription::HomeMaticParameter::Operations::Enum::event);
						parameter->logicalParameter.reset(new HmDeviceDescription::LogicalParameterAction(_bl));
						parameter->logicalParameter->type = HmDeviceDescription::LogicalParameter::Type::Enum::typeAction;
						parameter->physicalParameter->interface = HmDeviceDescription::PhysicalParameter::Interface::Enum::command;
						parameter->physicalParameter->type = HmDeviceDescription::PhysicalParameter::Type::Enum::typeInteger;
						parameter->physicalParameter->valueID = "BOOT";
						std::shared_ptr<HmDeviceDescription::PhysicalParameterEvent> eventFrame(new HmDeviceDescription::PhysicalParameterEvent());
						eventFrame->frame = "INFO_POWERON";
						parameter->physicalParameter->eventFrames.push_back(eventFrame);
						paramsetIterator->second->parameters.push_back(parameter);
					}
				}
			}

			for(std::map<uint32_t, std::shared_ptr<HmDeviceDescription::DeviceChannel>>::iterator i = homeMaticDevice->channels.begin(); i != homeMaticDevice->channels.end(); ++i)
			{
				if(!i->second) continue;
				std::map<HmDeviceDescription::ParameterSet::Type::Enum, std::shared_ptr<HmDeviceDescription::ParameterSet>>::iterator paramsetIterator = i->second->parameterSets.find(HmDeviceDescription::ParameterSet::Type::Enum::values);
				if(paramsetIterator != i->second->parameterSets.end() && paramsetIterator->second)
				{
					if(i->second->type != "SWITCH") continue;
					if(!paramsetIterator->second->getParameter("STATE")) continue;
					std::shared_ptr<HmDeviceDescription::HomeMaticParameter> parameter = paramsetIterator->second->getParameter("INHIBIT");
					if(parameter)
					{
						if(parameter->physicalParameter->eventFrames.empty())
						{
							std::shared_ptr<HmDeviceDescription::PhysicalParameterEvent> eventFrame(new HmDeviceDescription::PhysicalParameterEvent());
							eventFrame->frame = "INFO_POWERON";
							parameter->physicalParameter->eventFrames.push_back(eventFrame);
						}
					}
				}
			}
		}
		else if(filename == "rf_sec_sco.xml")
		{
			std::map<std::string, std::shared_ptr<HmDeviceDescription::DeviceFrame>>::iterator frameIterator = homeMaticDevice->framesByID.find("INFO_LEVEL_LOWBAT");
			if(frameIterator == homeMaticDevice->framesByID.end())
			{
				std::shared_ptr<HmDeviceDescription::DeviceFrame> frame(new HmDeviceDescription::DeviceFrame(_bl));
				frame->id = "INFO_LEVEL_LOWBAT";
				frame->direction = HmDeviceDescription::DeviceFrame::Direction::Enum::fromDevice;
				frame->allowedReceivers = HmDeviceDescription::DeviceFrame::AllowedReceivers::Enum::central;
				frame->isEvent = true;
				frame->type = 0x10;
				frame->subtype = 0x06;
				frame->subtypeIndex = 9;
				frame->fixedChannel = 0;
				HmDeviceDescription::HomeMaticParameter parameter(_bl);
				parameter.type = HmDeviceDescription::PhysicalParameter::Type::Enum::typeInteger;
				parameter.index = 12.7;
				parameter.size = 0.1;
				parameter.param = "LOWBAT";
				frame->parameters.push_back(parameter);
				homeMaticDevice->framesByID["INFO_LEVEL_LOWBAT"] = frame;
			}

			std::map<uint32_t, std::shared_ptr<HmDeviceDescription::DeviceChannel>>::iterator channelIterator = homeMaticDevice->channels.find(0);
			if(channelIterator != homeMaticDevice->channels.end() && channelIterator->second)
			{
				channelIterator->second->aesAlways = true;

				std::map<HmDeviceDescription::ParameterSet::Type::Enum, std::shared_ptr<HmDeviceDescription::ParameterSet>>::iterator paramsetIterator = channelIterator->second->parameterSets.find(HmDeviceDescription::ParameterSet::Type::Enum::values);
				if(paramsetIterator != channelIterator->second->parameterSets.end() && paramsetIterator->second)
				{
					std::shared_ptr<HmDeviceDescription::HomeMaticParameter> parameter = paramsetIterator->second->getParameter("LOWBAT");
					if(parameter)
					{
						if(parameter->physicalParameter->eventFrames.empty())
						{
							std::shared_ptr<HmDeviceDescription::PhysicalParameterEvent> eventFrame(new HmDeviceDescription::PhysicalParameterEvent());
							eventFrame->frame = "INFO_LEVEL_LOWBAT";
							parameter->physicalParameter->eventFrames.push_back(eventFrame);
						}
					}
				}
			}
		}

		HmDeviceDescription::HmConverter converter(_bl);
		std::shared_ptr<HomegearDevice> device(new HomegearDevice(_bl, _family));
		converter.convert(homeMaticDevice, device);
		return device;
	}
    catch(const std::exception& ex)
    {
    	_bl->out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(const Exception& ex)
    {
    	_bl->out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	_bl->out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return std::shared_ptr<HomegearDevice>();
}

std::shared_ptr<HomegearDevice> Devices::find(Systems::LogicalDeviceType deviceType, uint32_t firmwareVersion, int32_t countFromSysinfo)
{
	try
	{
		for(std::vector<std::shared_ptr<HomegearDevice>>::iterator i = _devices.begin(); i != _devices.end(); ++i)
		{
			for(SupportedDevices::iterator j = (*i)->supportedDevices.begin(); j != (*i)->supportedDevices.end(); ++j)
			{
				if((*j)->matches(deviceType, firmwareVersion))
				{
					if(countFromSysinfo > -1 && (*i)->dynamicChannelCountIndex > -1)
					{
						//Device has dynamic channel count
						for(std::vector<std::shared_ptr<HomegearDevice>>::iterator k = _dynamicDevices.begin(); k != _dynamicDevices.end(); ++k)
						{
							for(SupportedDevices::iterator l = (*k)->supportedDevices.begin(); l != (*k)->supportedDevices.end(); ++l)
							{
								if((*l)->matches(deviceType, firmwareVersion) && (*k)->getDynamicChannelCount() == countFromSysinfo) return *k;
							}
						}
						//No matching device was found
						std::shared_ptr<HomegearDevice> newDevice(new HomegearDevice(_bl, _family));
						*newDevice = **i;
						newDevice->setDynamicChannelCount(countFromSysinfo);
						_dynamicDevices.push_back(newDevice);
						return newDevice;
					}
					else return *i;
				}
			}
		}
	}
	catch(const std::exception& ex)
    {
     _bl->out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(const Exception& ex)
    {
     _bl->out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
     _bl->out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return nullptr;
}

// {{{ RPC
PVariable Devices::listKnownDeviceTypes(int32_t clientId, bool channels, std::map<std::string, bool>& fields)
{
	try
	{
		std::shared_ptr<Variable> descriptions(new Variable(VariableType::tStruct));

		for(std::vector<std::shared_ptr<HomegearDevice>>::iterator i = _devices.begin(); i != _devices.end(); ++i)
		{
			std::shared_ptr<Variable> description(new Variable(VariableType::tStruct));

			/*if(channel == -1) //Base device
			{
				BaseLib::DeviceDescription::PSupportedDevice supportedDevice = _rpcDevice->getType(_deviceType, _firmwareVersion);

				if(fields.empty() || fields.find("FAMILY") != fields.end()) description->structValue->insert(StructElement("FAMILY", std::shared_ptr<Variable>(new Variable((uint32_t)_deviceType.family()))));
				if(fields.empty() || fields.find("ID") != fields.end()) description->structValue->insert(StructElement("ID", std::shared_ptr<Variable>(new Variable((uint32_t)_peerID))));
				if(fields.empty() || fields.find("ADDRESS") != fields.end()) description->structValue->insert(StructElement("ADDRESS", std::shared_ptr<Variable>(new Variable(_serialNumber))));
				if(supportedDevice && !supportedDevice->longDescription.empty() && (fields.empty() || fields.find("DESCRIPTION") != fields.end())) description->structValue->insert(StructElement("DESCRIPTION", std::shared_ptr<Variable>(new Variable(supportedDevice->longDescription))));
				if(supportedDevice && !supportedDevice->serialPrefix.empty() && (fields.empty() || fields.find("SERIAL_PREFIX") != fields.end())) description->structValue->insert(StructElement("SERIAL_PREFIX", std::shared_ptr<Variable>(new Variable(supportedDevice->serialPrefix))));

				std::shared_ptr<Variable> variable = std::shared_ptr<Variable>(new Variable(VariableType::tArray));
				std::shared_ptr<Variable> variable2 = std::shared_ptr<Variable>(new Variable(VariableType::tArray));
				if(fields.empty() || fields.find("CHILDREN") != fields.end()) description->structValue->insert(StructElement("CHILDREN", variable));
				if(fields.empty() || fields.find("CHANNELS") != fields.end()) description->structValue->insert(StructElement("CHANNELS", variable2));

				if(fields.empty() || fields.find("CHILDREN") != fields.end() || fields.find("CHANNELS") != fields.end())
				{
					for(Functions::iterator i = _rpcDevice->functions.begin(); i != _rpcDevice->functions.end(); ++i)
					{
						if(!i->second->visible) continue;
						if(!i->second->countFromVariable.empty() && configCentral[0].find(i->second->countFromVariable) != configCentral[0].end() && configCentral[0][i->second->countFromVariable].data.size() > 0 && i->first >= i->second->channel + configCentral[0][i->second->countFromVariable].data.at(configCentral[0][i->second->countFromVariable].data.size() - 1)) continue;
						if(fields.empty() || fields.find("CHILDREN") != fields.end()) variable->arrayValue->push_back(std::shared_ptr<Variable>(new Variable(_serialNumber + ":" + std::to_string(i->first))));
						if(fields.empty() || fields.find("CHANNELS") != fields.end()) variable2->arrayValue->push_back(std::shared_ptr<Variable>(new Variable(i->first)));
					}
				}

				if(fields.empty() || fields.find("FIRMWARE") != fields.end())
				{
					if(_firmwareVersion != -1) description->structValue->insert(StructElement("FIRMWARE", std::shared_ptr<Variable>(new Variable(getFirmwareVersionString(_firmwareVersion)))));
					else if(!_firmwareVersionString.empty()) description->structValue->insert(StructElement("FIRMWARE", std::shared_ptr<Variable>(new Variable(_firmwareVersionString))));
					else description->structValue->insert(StructElement("FIRMWARE", std::shared_ptr<Variable>(new Variable(std::string("?")))));
				}

				if(fields.empty() || fields.find("AVAILABLE_FIRMWARE") != fields.end())
				{
					int32_t newFirmwareVersion = getNewFirmwareVersion();
					if(newFirmwareVersion > _firmwareVersion) description->structValue->insert(StructElement("AVAILABLE_FIRMWARE", std::shared_ptr<Variable>(new Variable(getFirmwareVersionString(newFirmwareVersion)))));
				}

				if(fields.empty() || fields.find("FLAGS") != fields.end())
				{
					int32_t uiFlags = 0;
					if(_rpcDevice->visible) uiFlags += 1;
					if(_rpcDevice->internal) uiFlags += 2;
					if(!_rpcDevice->deletable || isTeam()) uiFlags += 8;
					description->structValue->insert(StructElement("FLAGS", std::shared_ptr<Variable>(new Variable(uiFlags))));
				}

				if(fields.empty() || fields.find("INTERFACE") != fields.end()) description->structValue->insert(StructElement("INTERFACE", std::shared_ptr<Variable>(new Variable(getCentral()->getSerialNumber()))));

				if(fields.empty() || fields.find("PARAMSETS") != fields.end())
				{
					variable = std::shared_ptr<Variable>(new Variable(VariableType::tArray));
					description->structValue->insert(StructElement("PARAMSETS", variable));
					variable->arrayValue->push_back(std::shared_ptr<Variable>(new Variable(std::string("MASTER")))); //Always MASTER
				}

				if(fields.empty() || fields.find("PARENT") != fields.end()) description->structValue->insert(StructElement("PARENT", std::shared_ptr<Variable>(new Variable(std::string("")))));

				if(!_ip.empty() && (fields.empty() || fields.find("IP_ADDRESS") != fields.end())) description->structValue->insert(StructElement("IP_ADDRESS", std::shared_ptr<Variable>(new Variable(_ip))));

				if(fields.empty() || fields.find("PHYSICAL_ADDRESS") != fields.end()) description->structValue->insert(StructElement("PHYSICAL_ADDRESS", std::shared_ptr<Variable>(new Variable(_address))));

				//Compatibility
				if(fields.empty() || fields.find("RF_ADDRESS") != fields.end()) description->structValue->insert(StructElement("RF_ADDRESS", std::shared_ptr<Variable>(new Variable(_address))));
				//Compatibility
				if(fields.empty() || fields.find("ROAMING") != fields.end()) description->structValue->insert(StructElement("ROAMING", std::shared_ptr<Variable>(new Variable((int32_t)0))));

				if(fields.empty() || fields.find("RX_MODE") != fields.end()) description->structValue->insert(StructElement("RX_MODE", std::shared_ptr<Variable>(new Variable((int32_t)_rpcDevice->receiveModes))));

				if(!_rpcTypeString.empty() && (fields.empty() || fields.find("TYPE") != fields.end())) description->structValue->insert(StructElement("TYPE", std::shared_ptr<Variable>(new Variable(_rpcTypeString))));

				if(fields.empty() || fields.find("TYPE_ID") != fields.end()) description->structValue->insert(StructElement("TYPE_ID", std::shared_ptr<Variable>(new Variable(_deviceType.type()))));

				if(fields.empty() || fields.find("VERSION") != fields.end()) description->structValue->insert(StructElement("VERSION", std::shared_ptr<Variable>(new Variable(_rpcDevice->version))));

				if(fields.find("WIRELESS") != fields.end()) description->structValue->insert(StructElement("WIRELESS", std::shared_ptr<Variable>(new Variable(wireless()))));
			}
			else
			{
				if(_rpcDevice->functions.find(channel) == _rpcDevice->functions.end()) return Variable::createError(-2, "Unknown channel.");
				PFunction rpcFunction = _rpcDevice->functions.at(channel);
				if(!rpcFunction->countFromVariable.empty() && configCentral[0].find(rpcFunction->countFromVariable) != configCentral[0].end() && configCentral[0][rpcFunction->countFromVariable].data.size() > 0 && channel >= (int32_t)rpcFunction->channel + configCentral[0][rpcFunction->countFromVariable].data.at(configCentral[0][rpcFunction->countFromVariable].data.size() - 1)) return Variable::createError(-2, "Channel index larger than defined.");
				if(!rpcFunction->visible) return description;

				if(fields.empty() || fields.find("FAMILYID") != fields.end()) description->structValue->insert(StructElement("FAMILY", std::shared_ptr<Variable>(new Variable((uint32_t)_deviceType.family()))));
				if(fields.empty() || fields.find("ID") != fields.end()) description->structValue->insert(StructElement("ID", std::shared_ptr<Variable>(new Variable((uint32_t)_peerID))));
				if(fields.empty() || fields.find("CHANNEL") != fields.end()) description->structValue->insert(StructElement("CHANNEL", std::shared_ptr<Variable>(new Variable(channel))));
				if(fields.empty() || fields.find("ADDRESS") != fields.end()) description->structValue->insert(StructElement("ADDRESS", std::shared_ptr<Variable>(new Variable(_serialNumber + ":" + std::to_string(channel)))));

				if(fields.empty() || fields.find("AES_ACTIVE") != fields.end())
				{
					int32_t aesActive = 0;
					if(configCentral.find(channel) != configCentral.end() && configCentral.at(channel).find("AES_ACTIVE") != configCentral.at(channel).end() && !configCentral.at(channel).at("AES_ACTIVE").data.empty() && configCentral.at(channel).at("AES_ACTIVE").data.at(0) != 0)
					{
						aesActive = 1;
					}
					//Integer for compatability
					description->structValue->insert(StructElement("AES_ACTIVE", std::shared_ptr<Variable>(new Variable(aesActive))));
				}

				if(fields.empty() || fields.find("DIRECTION") != fields.end() || fields.find("LINK_SOURCE_ROLES") != fields.end() || fields.find("LINK_TARGET_ROLES") != fields.end())
				{
					int32_t direction = 0;
					std::ostringstream linkSourceRoles;
					std::ostringstream linkTargetRoles;
					for(LinkFunctionTypes::iterator k = rpcFunction->linkSenderFunctionTypes.begin(); k != rpcFunction->linkSenderFunctionTypes.end(); ++k)
					{
						//Probably only one direction is supported, but just in case I use the "or"
						if(!k->empty())
						{
							if(direction & 1) linkSourceRoles << " ";
							linkSourceRoles << *k;
							direction |= 1;
						}
					}
					for(LinkFunctionTypes::iterator k = rpcFunction->linkReceiverFunctionTypes.begin(); k != rpcFunction->linkReceiverFunctionTypes.end(); ++k)
					{
						//Probably only one direction is supported, but just in case I use the "or"
						if(!k->empty())
						{
							if(direction & 2) linkTargetRoles << " ";
							linkTargetRoles << *k;
							direction |= 2;
						}
					}

					//Overwrite direction when manually set
					if(rpcFunction->direction != Function::Direction::Enum::none) direction = (int32_t)rpcFunction->direction;
					if(fields.empty() || fields.find("DIRECTION") != fields.end()) description->structValue->insert(StructElement("DIRECTION", std::shared_ptr<Variable>(new Variable(direction))));
					if(fields.empty() || fields.find("LINK_SOURCE_ROLES") != fields.end()) description->structValue->insert(StructElement("LINK_SOURCE_ROLES", std::shared_ptr<Variable>(new Variable(linkSourceRoles.str()))));
					if(fields.empty() || fields.find("LINK_TARGET_ROLES") != fields.end()) description->structValue->insert(StructElement("LINK_TARGET_ROLES", std::shared_ptr<Variable>(new Variable(linkTargetRoles.str()))));
				}

				if(fields.empty() || fields.find("FLAGS") != fields.end())
				{
					int32_t uiFlags = 0;
					if(rpcFunction->visible) uiFlags += 1;
					if(rpcFunction->internal) uiFlags += 2;
					if(rpcFunction->deletable || isTeam()) uiFlags += 8;
					description->structValue->insert(StructElement("FLAGS", std::shared_ptr<Variable>(new Variable(uiFlags))));
				}

				if(fields.empty() || fields.find("GROUP") != fields.end())
				{
					int32_t groupedWith = getChannelGroupedWith(channel);
					if(groupedWith > -1)
					{
						description->structValue->insert(StructElement("GROUP", std::shared_ptr<Variable>(new Variable(_serialNumber + ":" + std::to_string(groupedWith)))));
					}
				}

				if(fields.empty() || fields.find("INDEX") != fields.end()) description->structValue->insert(StructElement("INDEX", std::shared_ptr<Variable>(new Variable(channel))));

				if(fields.empty() || fields.find("PARAMSETS") != fields.end())
				{
					std::shared_ptr<Variable> variable = std::shared_ptr<Variable>(new Variable(VariableType::tArray));
					description->structValue->insert(StructElement("PARAMSETS", variable));
					if(!rpcFunction->configParameters->parameters.empty()) variable->arrayValue->push_back(PVariable(new Variable(std::string("MASTER"))));
					if(!rpcFunction->variables->parameters.empty()) variable->arrayValue->push_back(PVariable(new Variable(std::string("VALUES"))));
					if(!rpcFunction->linkParameters->parameters.empty()) variable->arrayValue->push_back(PVariable(new Variable(std::string("LINK"))));
				}
				//if(rpcChannel->parameterSets.find(RPC::ParameterSet::Type::Enum::link) != rpcChannel->parameterSets.end()) variable->arrayValue->push_back(std::shared_ptr<Variable>(new Variable(rpcChannel->parameterSets.at(RPC::ParameterSet::Type::Enum::link)->typeString())));
				//if(rpcChannel->parameterSets.find(RPC::ParameterSet::Type::Enum::master) != rpcChannel->parameterSets.end()) variable->arrayValue->push_back(std::shared_ptr<Variable>(new Variable(rpcChannel->parameterSets.at(RPC::ParameterSet::Type::Enum::master)->typeString())));
				//if(rpcChannel->parameterSets.find(RPC::ParameterSet::Type::Enum::values) != rpcChannel->parameterSets.end()) variable->arrayValue->push_back(std::shared_ptr<Variable>(new Variable(rpcChannel->parameterSets.at(RPC::ParameterSet::Type::Enum::values)->typeString())));

				if(fields.empty() || fields.find("PARENT") != fields.end()) description->structValue->insert(StructElement("PARENT", std::shared_ptr<Variable>(new Variable(_serialNumber))));

				if(!_rpcTypeString.empty() && (fields.empty() || fields.find("PARENT_TYPE") != fields.end())) description->structValue->insert(StructElement("PARENT_TYPE", std::shared_ptr<Variable>(new Variable(_rpcTypeString))));

				if(fields.empty() || fields.find("TYPE") != fields.end()) description->structValue->insert(StructElement("TYPE", std::shared_ptr<Variable>(new Variable(rpcFunction->type))));

				if(fields.empty() || fields.find("VERSION") != fields.end()) description->structValue->insert(StructElement("VERSION", std::shared_ptr<Variable>(new Variable(_rpcDevice->version))));
			}*/
			descriptions->arrayValue->push_back(description);
		}
		return descriptions;
	}
	catch(const std::exception& ex)
    {
    	_bl->out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	_bl->out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	_bl->out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return Variable::createError(-32500, "Unknown application error.");
}
// }}}

}
}
