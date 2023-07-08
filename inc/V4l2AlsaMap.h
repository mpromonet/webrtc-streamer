/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** V4l2AlsaMap.h
**
** -------------------------------------------------------------------------*/

#pragma once

#ifndef WIN32
#include <string>
#include <cstring>
#include <map>
#include <filesystem>
#include <sstream>
#include <iostream>

/* ---------------------------------------------------------------------------
**  get "deviceid" 
** -------------------------------------------------------------------------*/
std::string getDeviceId(const std::string& devicePath) {
	std::string deviceid;
	std::filesystem::path fspath(devicePath);
	if (std::filesystem::exists(fspath))
	{
		std::string filename = std::filesystem::read_symlink(fspath).filename();
		std::stringstream ss(filename);
		getline(ss, deviceid, ':');
	}
	return deviceid;
}

std::map<std::string,std::string> getVideoDevices() {
	std::map<std::string,std::string> videodevices;
	std::string video4linuxPath("/sys/class/video4linux");
	for (auto const& dir_entry : std::filesystem::directory_iterator{video4linuxPath}) {
		std::string devname(dir_entry.path().filename());
		if (devname.find("video") == 0) {

			std::filesystem::path devicePath(dir_entry.path());
			devicePath.append("device");
			std::string deviceid = getDeviceId(devicePath);

			if (!deviceid.empty()) {
				int deviceNumber = atoi(devname.substr(strlen("video")).c_str());
				std::string devicename = "videocap://";
				devicename += std::to_string(deviceNumber);				
				videodevices[devicename] = deviceid;
			}
		}
	}
	return videodevices;
}

std::map<std::string,std::string> getAudioDevices() {
	std::map<std::string,std::string> audiodevices;
	std::string audioLinuxPath("/sys/class/sound");
	for (auto const& dir_entry : std::filesystem::directory_iterator{audioLinuxPath}) {
		std::string devname(dir_entry.path().filename());
		if (devname.find("card") == 0) {
			std::filesystem::path devicePath(dir_entry.path());
			devicePath.append("device");
			std::string deviceid = getDeviceId(devicePath);

			if (!deviceid.empty()) {
				int deviceNumber = atoi(devname.substr(strlen("card")).c_str());
				std::string devicename = "audiocap://";
				devicename += std::to_string(deviceNumber);
				audiodevices[deviceid] = devicename;
			}				
		}			
	}
	return audiodevices;
}

std::map<std::string,std::string>  getV4l2AlsaMap() {
	std::map<std::string,std::string> videoaudiomap;

	std::map<std::string,std::string> videodevices = getVideoDevices();
	std::map<std::string,std::string> audiodevices = getAudioDevices();

	for (auto & id : videodevices) {
		auto audioDevice = audiodevices.find(id.second);
		if (audioDevice != audiodevices.end()) {
			std::cout <<  id.first << "=>" << audioDevice->second << std::endl;
			videoaudiomap[id.first] = audioDevice->second;
		}
	}
	
	return videoaudiomap;
}
#else
std::map<std::string,std::string>  getV4l2AlsaMap() { return std::map<std::string,std::string>(); };
#endif
