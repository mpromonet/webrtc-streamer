# build
FROM mcr.microsoft.com/windows/servercore:1803 as builder
LABEL maintainer=michel.promonet@free.fr

WORKDIR /webrtc-streamer
COPY . /webrtc-streamer

ENV DEPOT_TOOLS_WIN_TOOLCHAIN=0 \
       CLANG_VERSION=ToT \
       PYTHONIOENCODING=UTF-8 \
       GYP_MSVS_OVERRIDE_PATH=C:\\BuildTools

# Install Chocolatey & packages
RUN powershell.exe -NoProfile -InputFormat None -ExecutionPolicy Bypass -Command "iex ((New-Object System.Net.WebClient).DownloadString('https://chocolatey.org/install.ps1'))" && SETX PATH "%PATH%;%ALLUSERSPROFILE%\chocolatey\bin" 
RUN choco install -y git python2 sed curl \
       && choco install -y cmake --installargs 'ADD_CMAKE_TO_PATH=User' \
       && choco install -y visualstudio2017buildtools --package-parameters "--add Microsoft.VisualStudio.Workload.VCTools --installPath %GYP_MSVS_OVERRIDE_PATH%" || IF "%ERRORLEVEL%"=="3010" EXIT 0   

# install WebRTC 
RUN git config --global core.autocrlf false \
       && git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git C:\depot_tools \
       && SETX PATH "%PATH%;C:\depot_tools" 
# workaround bootstraping that delete the cipd.ps1 file       
RUN powershell -NoProfile -ExecutionPolicy RemoteSigned -Command C:\depot_tools\cipd.ps1 -CipdBinary C:\depot_tools\.cipd_client.exe -BackendURL https://chrome-infra-packages.appspot.com -VersionFile C:\depot_tools\cipd_client_version   

RUN  mkdir c:\webrtc && cd c:\webrtc \
       && fetch --no-history --nohooks webrtc \
       && sed -i -e "s|'src/resources'],|'src/resources'],'condition':'rtc_include_tests==true',|" src/DEPS 

RUN call "C:\BuildTools\VC\Auxiliary\Build\vcvars64.bat" \
       && gclient sync \
       && SETX PATH "%PATH%;C:\webrtc\src\third_party\llvm-build\Release+Asserts\bin"

# build
RUN cmake -G Ninja -DCMAKE_C_COMPILER=clang-cl.exe -DCMAKE_CXX_COMPILER=clang-cl.exe . && ninja && cpack 

EXPOSE 8000
ENTRYPOINT [ "webrtc-streamer.exe" ]
CMD [ "-a", "-C", "config.json", "screen://" ]
