# asinello-client
Client software of the asinello project

## CLion

We are using CLion with Platform IO as our IDE. When I set that up, I had some trouble: The environment did not show up as a target so I couldn't deploy the binary to the device and library imports did not work.

### Library header imports not working

This did the trick for me: https://community.platformio.org/t/arduino-h-not-found/17127/11

Basically I switched from Visual Studio toolchain to MinGW and manually set the PlatformIO C and C++ compilers.

### No target

Unfortunately I don't know how this issue was fixed. Just before it worked, I had completely removed all run configurations and CMake profiles. I then created a single CMake profile reflecting my target PlatformIO environment. It should be filled automatically but for me that was not the case (it was before but not when I finally went for the fix). So I just manually entered the build type (`d1_mini` by the time writing this). Then I created the run configurations and everything worked. Good luck! ;)
