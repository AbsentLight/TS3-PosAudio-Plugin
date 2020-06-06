# TS3-PosAudio-Plugin

This is the Teamspeak client plugin for use with [MC-PosAudio-Plugin](https://github.com/AbsentLight/MC-PosAudio-Plugin).

### Setting up the project in Windows:

1.Clone this project

2.Download and install [VCPKG](https://github.com/microsoft/vcpkg) - See the VCPKG README.md for installation instructions

3.Once VCPKG is installed run 'vcpkg install cpprestsdk:x64-windows-static' and 'vcpkg install cpprestsdk:x86-windows-static' in CMD in the folder of VCPKG

4.You can now open this project in Visual Studio by opening the .sln file, the cpprest library should be linked as all other configurations should already be reflected in the project. 

### Configuring the plugin:
You'll need to set up a Bukkit/Bukkit plugin compatible server running the [MC-PosAudio-Plugin](https://github.com/AbsentLight/MC-PosAudio-Plugin), once you've got that you need to alter the channel in Teamspeak you want to use with positional audio with the following as a description (preserve the pipes, replace everything inside and including the less/greater than symbols with values based on your server configuration):

    |<url/ip>|<port>|

### Building and using the plugin:
Building the plugin dll should cause it to be automatically copied to your teamspeak3 plugin folder and teamspeak should open (Teamspeak will need to be closed for this otherwise you'll get a build error). If it successfully built then it should be in Tools > Options > Addons list where it should show up with no red errors - if there is a red error it will not work and something has gone wrong, this is probably a libary linking issue if in the log it says error 126. 
If you want to distribute the plugin to others just send them the CPP-PAR.dll to be put in their %appdata%\TS3Client\plugins folder - alternatively there is a way of packaging it up so it can be double clicked and installed by Teamspeak automatically (this is how packaged releases will be distributed).

### Planned for the future:

1.Better exception handling, you may encounter occasional crashes

