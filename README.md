# einherjarSDVX
is a fork of [Unnamed SDVX clone](https://github.com/Drewol/unnamed-sdvx-clone), which aims to improve Modchart freedom.

### Current features:
- Realtime Audio Spectrum
- Additional Audio Effects:
	- Vocal

<!---	### Other changes to Unnamed SDVX clone		-->
<!---	-						-->

## How to build:

### Windows:
It is not required to build from source. A download link to a pre-built copy of the game is located at the beginning of this README.
The recommended Visual Studio version is 2017, if you want to use a different version then you
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
