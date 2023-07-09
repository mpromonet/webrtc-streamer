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
#include <linux/videodev2.h>
#include <fcntl.h>
#include <sys/ioctl.h>

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

std::map<int,int> videoDev2Idx() {
  std::map<int,int> dev2idx;
  uint32_t count = 0;
  char device[20];
  int fd = -1;
  struct v4l2_capability cap;

  for (int devId = 0; devId < 64; devId++) {
    snprintf(device, sizeof(device), "/dev/video%d", devId);
    if ((fd = open(device, O_RDONLY)) != -1) {
      if (ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0 ||
          !(cap.device_caps & V4L2_CAP_VIDEO_CAPTURE)) {
        close(fd);
        continue;
      } else {
		close(fd);
		dev2idx[devId] = count;
		count++;
	  }
    }
  }
  return dev2idx;
}



std::map<std::string,std::string> getVideoDevices() {
	std::map<int,int> dev2Idx = videoDev2Idx();
	std::map<std::string,std::string> videodevices;
	std::string video4linuxPath("/sys/class/video4linux");
	if (std::filesystem::exists(video4linuxPath)) {
		for (auto const& dir_entry : std::filesystem::directory_iterator{video4linuxPath}) {
			std::string devname(dir_entry.path().filename());
			if (devname.find("video") == 0) {

				std::filesystem::path devicePath(dir_entry.path());
				devicePath.append("device");
				std::string deviceid = getDeviceId(devicePath);

				if (!deviceid.empty()) {
					int deviceNumber = atoi(devname.substr(strlen("video")).c_str());
					std::string devicename = "videocap://";
					devicename += std::to_string(dev2Idx[deviceNumber]);				
					videodevices[devicename] = deviceid;
				}
			}
		}
	}
	return videodevices;
}

std::map<std::string,std::string> getAudioDevices() {
	std::map<std::string,std::string> audiodevices;
	std::string audioLinuxPath("/sys/class/sound");
	if (std::filesystem::exists(audioLinuxPath)) {
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
