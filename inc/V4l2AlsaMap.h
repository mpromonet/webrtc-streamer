/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** V4l2AlsaMap.h
**
** -------------------------------------------------------------------------*/

#pragma once

#if HAVE_ALSA
#include <dirent.h>
#include <alsa/asoundlib.h>

/* ---------------------------------------------------------------------------
**  get a "deviceid" from uevent sys file
** -------------------------------------------------------------------------*/
std::string getDeviceId(const std::string& evt) {
    std::string deviceid;
    std::istringstream f(evt);
    std::string key;
    while (getline(f, key, '=')) {
            std::string value;
	    if (getline(f, value)) {
		    if ( (key =="PRODUCT") || (key == "PCI_SUBSYS_ID") ) {
			    deviceid = value;
			    break;
		    }
	    }
    }
    return deviceid;
}

std::map<std::string,std::string>  getV4l2AlsaMap() {
	std::map<std::string,std::string> videoaudiomap;

	std::map<std::string,std::string> videodevices;
	std::string video4linuxPath("/sys/class/video4linux");
	DIR *dp = opendir(video4linuxPath.c_str());
	if (dp != NULL) {
		struct dirent *entry = NULL;
		while((entry = readdir(dp))) {
			std::string devicename;
			std::string deviceid;
			if (strstr(entry->d_name,"video") == entry->d_name) {
				std::string devicePath(video4linuxPath);
				devicePath.append("/").append(entry->d_name).append("/name");
				std::ifstream ifsn(devicePath.c_str());
				devicename = std::string(std::istreambuf_iterator<char>{ifsn}, {});
				devicename.erase(devicename.find_last_not_of("\n")+1);

				std::string ueventPath(video4linuxPath);
				ueventPath.append("/").append(entry->d_name).append("/device/uevent");
				std::ifstream ifsd(ueventPath.c_str());
				deviceid = std::string(std::istreambuf_iterator<char>{ifsd}, {});
				deviceid.erase(deviceid.find_last_not_of("\n")+1);
			}

			if (!devicename.empty() && !deviceid.empty()) {
				videodevices[devicename] = getDeviceId(deviceid);
			}
		}
		closedir(dp);
	}

	std::map<std::string,std::string> audiodevices;
	int rcard = -1;
	while ( (snd_card_next(&rcard) == 0) && (rcard>=0) ) {
		void **hints = NULL;
		if (snd_device_name_hint(rcard, "pcm", &hints) >= 0) {
			void **str = hints;
			while (*str) {
				std::ostringstream os;
				os << "/sys/class/sound/card" << rcard << "/device/uevent";

				std::ifstream ifs(os.str().c_str());
				std::string deviceid = std::string(std::istreambuf_iterator<char>{ifs}, {});
				deviceid.erase(deviceid.find_last_not_of("\n")+1);
				deviceid = getDeviceId(deviceid);

				if (!deviceid.empty()) {
					if (audiodevices.find(deviceid) == audiodevices.end()) {
						std::string audioname = snd_device_name_get_hint(*str, "DESC");
						std::replace( audioname.begin(), audioname.end(), '\n', '-');
						audiodevices[deviceid] = audioname;
					}
				}

				str++;
			}

			snd_device_name_free_hint(hints);
		}
	}

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