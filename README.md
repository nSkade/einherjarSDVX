# einherjarSDVX
is an experimental fork of [Unnamed SDVX clone](https://github.com/Drewol/unnamed-sdvx-clone), which aims to improve Modchart freedom.

### Current features:
- extensive spline based mod system
	- access via custom Background lua
	- variable spline Point amount and position
	- multiple interpolation types:
		- linear
		- cos
		- none
		- cubic
	- Track, Hold and Laser support
	- Toggle Specific Lanes
	- no limitations in degrees of freedom (layered Translation, Rotation, Scale)
- reworked Practice Mode for chart editing preview
	- reload lua bg and chart on the fly (F11)
- realtime audio spectrum
- additional audio effects:
	- vocal filter
	
- minor quality of life improvements
	- repeat key on hold in navigation
	- more command line options:
		- ```-console``` view log in cmd
		- ```-cExit``` confirm exit
		- ```-practice``` start in practice Mode (or press shift in song select)

<!---	### Other changes to Unnamed SDVX clone		-->
<!---	-						-->

## How to build:

### Windows:
The recommended Visual Studio version is 2019, if you want to use a different version then you
will need to edit the 'GenerateWin64ProjectFiles.bat' if you want to follow the guide below.

0. Clone the project using `git` and then run `git submodule update --init --recursive` to download the required submodules.
1. Install [CMake](https://cmake.org/download/)
2. Install [vcpkg](https://github.com/microsoft/vcpkg)
3. Install the packages listed in 'build.windows'
4. Run 'GenerateWin64ProjectFiles.bat' from the root of the project
    * If this fails, try using the `-DCMAKE_TOOLCHAIN_FILE=[VCPKG_ROOT]\scripts\buildsystems\vcpkg.cmake` flag that vcpkg should give you on install
5. Build the generated Visual Studio project 'FX.sln'
6. Run the executable made in the 'bin' folder

To run from Visual Studio, go to Properties for Main > Debugging > Working Directory and set it to '$(OutDir)' or '..\\bin'

### Linux:
0. Clone the project using `git` and then run `git submodule update --init --recursive` to download the required submodules.
1. Install [CMake](https://cmake.org/download/)
2. Check 'build.linux' for libraries to install
3. Run `cmake -DCMAKE_BUILD_TYPE=Release .` and then `make` from the root of the project
4. Run the executable made in the 'bin' folder

### macOS:
0. Clone the project using `git` and then run `git submodule update --init --recursive` to download the required submodules.
1. Install dependencies
	* [Homebrew](https://github.com/Homebrew/brew): `brew install cmake freetype libvorbis sdl2 libpng jpeg libarchive libiconv`
2. Run `mac-cmake.sh` and then `make` from the root of the project.
3. Run the executable made in the 'bin' folder.
