Nothing here.  
这里什么也没有。  
ここには何もありません。  

Uch
===
Computer Networking Lab 1  
A tiny C/S chatting application   

### Brief
Supports online chatting (TCP, P2P).  
Supports offline messages (TCP, server relay).  
Supports online file transmitting (UDP, P2P).  

### License
The code is [unlicensed](http://unlicense.org/).  

### Dependencies
* [`nana`](http://nanapro.org/en-us/) ([modified](https://github.com/EAirPeter/nana) in order to add user event)  
  Providing GUIs  
* [`Crypto++`](https://www.cryptopp.com/)  
  Providing AES 256-CFB and SHA-256  
* [`yaml-cpp`](https://github.com/jbeder/yaml-cpp)  
  Providing configuration files  

Their licenses are placed [here](Dependencies/).  

### Notes - part 0
[`UchClient`](UchClient/) provides the client.  
[`UchServer`](UchServer/) provides the server.  
[`UchCommon`](UchCommon/) provides wheels.  
[`UchProtocol`](UchProtocol/) provides application layer protocols for this application.  
[`UchFile`](UchFile/) provides the file transmitting routine.  

### Notes - part 1
P2P file transmitting is implemented over a handmade RDT protocol called [Ucp](UchCommon/Ucp.hpp).  
[`UchFile`](UchFile/) is called as a child process.  
Windows Socket 2 and Windows API plays an important role in this application.  
Again, the `nana` GUI library is [modified](https://github.com/EAirPeter/nana).  

### Notes - part 2
**Do not write code as ugly as I do.**  
It is not recommended that copying my code directly into your assignments or labs.  
This application only supports Windows platform.  
This application requires at least Windows 7.  
The Win32 version is not tested.  
