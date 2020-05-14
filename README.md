# TS3-PosAudio-Plugin

This is the Teamspeak client plugin for use with [MC-PosAudio-Plugin](https://github.com/AbsentLight/MC-PosAudio-Plugin).

### Setting up the project in Windows:

1.Clone this project

2.Download and install [VCPKG](https://github.com/microsoft/vcpkg) - See the VCPKG README.md for installation instructions

3.Once VCPKG is installed run 'vcpkg install cpprestsdk:x64-windows-static' and 'vcpkg install cpprestsdk:x86-windows-static' in CMD in the folder of VCPKG

4.You can now open this project in Visual Studio by opening the .sln file, the cpprest library should be linked as all other configurations should already be reflected in the project. 

### Configuring the plugin:
You'll need to set up a Bukkit/Bukkit plugin compatible server running the [MC-PosAudio-Plugin](https://github.com/AbsentLight/MC-PosAudio-Plugin), once you've got that you need to point the below line at the address of the minecraft server e.g. alter `192.168.2.58` to be the address of the server, you can find the line in `plugin.cpp`.

```web::http::client::http_client client(U("http://192.168.2.58:9000/request"));```

Once you've updated the above line you can build the solution and the plugin dll should get automatically copied to your teamspeak3 plugin folder and teamspeak should open. If it successfully built then it should be in Tools > Options > Addons list where it should show up with no red errors - if there is a red error it will not work and something has gone wrong, this is probably a libary linking issue if in the log it says error 126. If you want to distribute the plugin to others just send them the CPP-PAR dll to be put in their %appdata%\TS3Client\plugins folder.

### Planned for the future:

1.Settings menu in Teamspeak to set the MC server address

2.Better exception handling, this plugin crashes Teamspeak if it can't find the server

3.Customisability of rolloff curve for distance with in Teamspeak or on the server
