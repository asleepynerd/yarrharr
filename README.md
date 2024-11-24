# YarrHarr

A command-line interface tool for managing media downloads using TMDB metadata.

## Features

- Search for movies and TV shows
- View detailed information about movies and shows
- Download movies and TV shows (individual episodes, seasons, or entire series)
- Optional MP4 conversion support (requires FFmpeg)
- Cross-platform support (Windows, macOS, Linux)

## Dependencies

### Required

- C++17 compatible compiler
- CMake (3.15 or newer)
- libcurl
- nlohmann-json

### Optional

- FFmpeg (for MP4 conversion)

## Installation

### macOS

1. Install dependencies using Homebrew:

```sh
brew install cmake libcurl nlohmann-json ffmpeg
```

2. Clone the repository:

```sh
git clone https://github.com/asleepynerd/yarrharr.git
```

3. Build the project (or use provided files in releases)

```sh
cd yarrharr
mkdir build
cd build
cmake ..
make
```

4. Run the program:

```sh
./yarrharr
```

### Linux (Debian/Ubuntu)

1. Install dependencies using apt:

```sh
sudo apt install cmake libcurl-dev nlohmann-json-dev ffmpeg
```

2. Clone the repository:

```sh
git clone https://github.com/asleepynerd/yarrharr.git
```

3. Build the project (or use provided files in releases)

```sh
cd yarrharr
mkdir build
cd build
cmake ..
make
```

4. Run the program:

```sh
./yarrharr
```

### Windows

1. Install prerequisites:

   - [Visual Studio](https://visualstudio.microsoft.com/downloads/) with C++ development tools
   - [CMake](https://cmake.org/download/)
   - [vcpkg](https://github.com/microsoft/vcpkg#quick-start-windows)

2. Install dependencies using vcpkg:

```sh
.\vcpkg install curl:x64-windows
.\vcpkg install nlohmann-json:x64-windows
```

3. Clone the repository:

```sh
git clone https://github.com/asleepynerd/yarrharr.git
```

4. Build the project (You will not recieve any support for building on Windows, so you will need to build it yourself, issues are welcome on GitHub)

```sh
cd yarrharr
mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=../vcpkg/scripts/buildsystems/vcpkg.cmake
make
```

5. Run the program:

```sh
.\yarrharr.exe
```

## Configuration

The program will automatically create a `config.json` file in the same directory as the executable. You can modify the settings in this file to customize the program's behavior. You will need to obtain an API key from [The Movie Database (TMDB)](https://www.themoviedb.org/settings/api) to use the search and download features.

## Usage

```sh
./yarrharr
```

## License

This project is licensed under the AGPL-3.0 License. See the [LICENSE](LICENSE) file for details.
