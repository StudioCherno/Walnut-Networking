# Walnut-Networking

Walnut-Networking an optional module for [Walnut](https://github.com/StudioCherno/Walnut) which provides networking capabilities.

## Getting Started
Add this is a submodule to your [Walnut](https://github.com/StudioCherno/Walnut) application. If you need to make a new Walnut app, check out [WalnutAppTemplate](https://github.com/StudioCherno/WalnutAppTemplate).

If cloned into `Walnut/` directory, Walnut's build scripts should automatically pick this up and include it into your project if present. Otherwise, if doing this manually, include the `Build-Walnut-Networking.lua` file into your build scripts somewhere, eg:

``` lua
include "Walnut/Walnut-Networking/Build-Walnut-Networking.lua"
```

[Walnut Chat](https://github.com/TheCherno/Walnut-Chat) is an example app made using Walnut and this Walnut-Networking module, so check that out for a further example of how to get started. The server component of this project runs on a Linux server (headless) which is also a useful example.

## Features
- Supports Windows and Linux (mostly)
- Client/Server API for both reliable and unreliable data transmission, using Valve's [GameNetworkingSockets](https://github.com/ValveSoftware/GameNetworkingSockets) library
- Easy and clean network event callbacks and connection management
- DNS lookup utility function for translating domain names to IP addresses (`Walnut::Utils::ResolveDomainName`)
- _[Planned]_ HTTP API for GET/POST requests 

### 3rd Party Libraries
- [GameNetworkingSockets](https://github.com/ValveSoftware/GameNetworkingSockets)