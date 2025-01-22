# RetroFE - A cross-platform frontend for MAME cabinets

[Project Discord](https://discord.gg/dpcsP8Hm9W) | [GitHub Wiki](https://github.com/CoinOPS-Official/RetroFE/wiki) | [Changelog](CHANGELOG.md)

RetroFE is a cross-platform desktop frontend designed for MAME cabinets and game centers, with a focus on simplicity and customization. 
This repository is actively maintained and hundreds of commits ahead of the original RetroFE project. 
It is designed for use within CoinOPS builds, bringing with it a significant increase in performance, optimisations, and available feature set. 

It's licensed under the terms of the GNU General Public License, version 3 or later (GPLv3).

## What's so special about this fork?
* Performance and optimisations
	* 64bit codebase
    * C++17 as standard
	* Modern render engine; DX11 for Windows, Metal for MacOS
 	* Hardware accelerated video support for Windows
	* VSync and support for high refresh rate
	* Metadata database build time reduced
	* File caching to prevent drive lashing
	* RAM usage reduced by 70%
* Features
	* Ability to start on random item; fed up of seeing the same game every time?
 	* Robust video marquee and 2nd screen support	 
	* Upgraded attract mode
	* Upgraded favouriting system; global and local favourites
	* Start and exit scripts; run programs such as steam at retrofe launch
	* In depth logging system; 7 logging levels
	* Kiosk mode; lock things down for kids or cleanliness
	* And much more!

## System Requirements
* OS
    * Windows (10 or higher)
    * Linux
    * macOS (11 Big Sur or higher)
	* Unix-like systems other than Linux are not officially supported but may work
* Processor
    * A modern CPU (2014 or later) is highly recommended
* Graphics
    * A reasonably modern graphics card (Direct3D 11+ / OpenGL 4+ / Metal on MacOS)

#   Building for Windows #
### Install libraries
 
* Install Python (https://www.python.org/downloads/windows/)
* Install sphinx with python (https://www.sphinx-doc.org/en/1.6.5/install.html)
* Install visual studio 2019 (https://visualstudio.microsoft.com/downloads/)
* Install Microsoft Windows SDK for Windows 10 and .net Framework 4 (https://developer.microsoft.com/nl-nl/windows/downloads/windows-10-sdk/)
* Install cmake (https://cmake.org/download/)
* Install git (https://git-scm.com/download/win)
* Install 7zip (https://www.7-zip.org/)
* Install gstreamer and gstreamer-devel to `c:/gstreamer(x86-64 bit)` (https://gstreamer.freedesktop.org/download/#windows)

### Download and compile the source code
Download the source code

	git clone https://github.com/CoinOPS-Official/RetroFE.git

Setup Environment (to setup necessary variables and paths to compile in visual studio)

	cd retrofe

Generate visual studio solution files

	cmake -A x64 -B .\RetroFE\Build -D GSTREAMER_ROOT=C:\gstreamer\1.0\msvc_x86_64 -S .\RetroFE\Source
  
Compile RetroFE

	cmake --build RetroFE/Build --config Release

#   Building for Linux #

### Install libraries
Install necessary dependencies:

 #### Debian
```bash
sudo apt-get install git g++ cmake dos2unix zlib1g-dev
libsdl2-2.0 libsdl2-mixer-2.0 libsdl2-image-2.0 libsdl2-ttf-2.0
libsdl2-dev libsdl2-mixer-dev libsdl2-image-dev libsdl2-ttf-dev
libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev libgstreamer-plugins-good1.0-dev gstreamer1.0-libav
zlib1g-dev libglib2.0-0 libglib2.0-dev sqlite3
```

#### Fedora
```bash
sudo dnf install git gcc-c++ cmake dos2unix zlib-devel
SDL2 SDL2_mixer SDL2_image SDL2_ttf
SDL2-devel SDL2_mixer-devel SDL2_image-devel SDL2_ttf-devel
gstreamer1 gstreamer1-plugins-base gstreamer1-plugins-good gstreamer1-libav
libglib2 glib2-devel sqlite
```

#### Arch
```bash
sudo pacman -S git gcc cmake dos2unix zlib
sdl2 sdl2_mixer sdl2_image sdl2_ttf
gstreamer gst-plugins-base gst-plugins-good gst-libav
glib2 sqlite
```

### Download and compile the source code
Download the source code

	git clone https://github.com/CoinOPS-Official/RetroFE.git

Generate your gcc make files

	cd retrofe
	cmake RetroFE/Source -BRetroFE/Build

Compile RetroFE

	cmake --build RetroFE/Build

#   Building for MacOS #
## Universal2 Binaries

An Xcode project has been created to build universal binaries (x86_64 and arm64)

### Install libraries

Download the following .dmg and place all .framework's in `RetroFE/ThirdPartyMac`

* Install SDL2 (https://github.com/libsdl-org/SDL/releases/latest)
* Install SDL2\_image (https://github.com/libsdl-org/SDL_image/releases/latest)
* Install SDL2\_mixer (https://github.com/libsdl-org/SDL_mixer/releases/latest)
* Install SDL2\_ttf (https://github.com/libsdl-org/SDL_ttf/releases/latest)
* Install Gstreamer (https://gstreamer.freedesktop.org/download/#macos)
* * For Gstreamer both runtime and dev packages are needed, they are installed to `Macintosh HD/Library/Frameworks` and should be moved to `RetroFE/ThirdPartyMac`

### Download and compile the source code
Download the source code

	git clone https://github.com/CoinOPS-Official/RetroFE.git

Open the Xcodeproj in `RetroFE/xcode` and build target

#   Optional #

###   Creating a test environment

A launchable test environment can be created with the following commands 

	python Scripts/Package.py --os=windows/linux/mac --build=full

Copy your live RetroFE system to any folder of your choosing. Files can be found in `Artifacts/{os}/RetroFE`

### Set $RETROFE_PATH via Environment variable 

RetroFE will load it's media and configuration files relative to where the binary file is located. This allows the build to be portable. If you want RetroFE to load your configuration from a fixed location regardless of where your install is copy your configuration there and set $RETROFE_PATH. Note this will work if you start RetroFE from the command line.

	vi ~/.bash_profile
	export RETROFE_PATH=/your/new/retrofe


### Set RETROFE_PATH via flat file 

Depending on your version of OS X the GUI will read user defined Environment variables from [another place](http://stackoverflow.com/questions/135688/setting-environment-variables-in-os-x). If you find this dificult to setup you can get around it by creating a text file in your HOME directory: /Users/<you>/.retrofe with one line no spaces: /your/new/retrofe. This will also work in Linux. RetroFE's configuration search order is 1st: ENV, Flat file, and executable location.

	echo /your/new/retrofe > ~/.retrofe

### Fix libpng iCCP warnings

The issue is with the png files that are being used with the Artwork. Libpng is pretty touchy about it. You can get rid of these messages with a handy tool called pngcrush found on sourceforge and github.

Error message:
	
	libpng warning: iCCP: known incorrect sRGB profile


Install pngcrush on Mac:    (linux use apt-get ?)
	
	brew install pngcrush


Use pngcrush to Find and repair pngs: 
	
	find /usr/local/opt/retrofe/collections -type f -iname '*.png' -exec pngcrush -ow -rem allb -reduce {} \;

