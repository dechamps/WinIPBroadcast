README
WinIPBroadcast 1.2
By Etienne Dechamps <etienne@edechamps.fr>
https://github.com/dechamps/WinIPBroadcast

BASIC USAGE

You just need to install it. WinIPBroadcast runs as a service which will be automatically started at the end of the installation. There is no configuration, It Just Works (TM).

To control WinIPBroadcast, use the Service Manager in the Administration Tools folder of the Control Panel.

COMMAND LINE USAGE

You can use WinIPBroadcast on the command line using these parameters:

WinIPBroadcast install - installs the service.
WinIPBroadcast remove - removes the service.
WinIPBroadcast run - runs WinIPBroadcast as a console application. Useful for diagnostics (error messages will be shown).

DESCRIPTION

Windows 7 only sends global IP broadcast packets (destination address 255.255.255.255) on the interface which matches the preferred route for 255.255.255.255 in the route table.

More often than not, this isn't what you want: software that makes use of global broadcast packets (e.g. server browsers, games) expect these packets to be sent to all interfaces in order to reach all the local hosts your computer has access to.

For example, consider a computer which is simultaneously on a LAN and on a VPN. The user is a video game player who sometimes use multiplayer on the LAN, sometimes on the VPN with a few friends. Unfortunately, the game uses global broadcast packets to discover open games on the local network. Consequently, the player will only be able to see games on one network: the LAN or the VPN, depending on his routing table (most probably the LAN). He can modify his routing table to see games on the VPN, but he won't be able to see both at the same time.

WinIPBroadcast is an extremely small program (15 KB executable) which has been specifically designed to solve this problem. It works using an interesting fact: it is possible to receive locally generated global broadcast packets when listening on the loopback address (127.0.0.1). WinIPBroadcast listens on the local address for all broadcast using RAW sockets, then for each broadcast packet, it relays it to all interfaces except the prefered one.

Thus, if an application send a broadcast packet, Windows will send it to the preferred interface, then WinIPBroadcast will receive it and resend it to all other interfaces. In the end the packet is sent on all network interfaces: problem solved.

WinIPBroadcast has not (yet) been tested on platforms other than Windows 7. Theoratically, it should work as well on Windows 2000/XP/2003/Vista/2008.

AUTHOR

Etienne Dechamps (a.k.a e-t172)
etienne@edechamps.fr
