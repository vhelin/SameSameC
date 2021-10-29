
# Requirements

For the linker to work you'll need to install WLA DX (https://github.com/vhelin/wla-dx) v10.0+ as the linker uses WLA to assemble the generated assembly files.

# Install

Build the executables.

## Linux (and other UNIXes as well and Cygwin and alike?)

Execute "build.sh" and the script will build (using make and gcc) both compiler and linker, and copies both executables to "binaries" folder.

## Amiga

Execute "build_amiga" and the script will build (using smake and SAS/C compiler) both compiler and linker, and copies both executables to "binaries" folder.

## Windows

Open "windows/SameSameC.sln" in Visual Studio and build.

# Other platforms

Feel free to make a pull request with build scripts for other platforms.
