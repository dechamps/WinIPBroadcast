WinIPBroadcast changelog
========================

- 1.5 (09/04/2017)
  - Fix a regression introduced in 1.4 where WinIPBroadcast would not only relay packets from the local machine, but broadcast packets arriving on network interfaces as well.
- 1.4 (09/04/2017)
  - Don't relay packets with a TTL of 1 or less.
  - Relayed packets will now be sent with a TTL of 1.
  - Use TTL instead of source address for the purposes of preventing recursive relaying. This removes a limitation of WinIPBroadcast where it would not relay packets that are sent with a source address different from the global broadcast default route.
- 1.3 (24/08/2014)
  - Harden service security by removing most of its privileges. This prevents a potential attacker from gaining highly privileged access to the system by compromising WinIPBroadcast.
- 1.2 (15/11/2009)
  - Forgot to close a socket when sending broadcasts, which could cause serious problems on the system (e.g. total freezes) when sending large numbers of broadcast packets.
- 1.1 (31/10/2009)
  - Handle `WSAEHOSTUNREACH` correctly in `broadcastRouteAddress()`. Allows WinIPBroadcast to survive waking from sleep.
- 1.0 (21/10/2009)
  - First public release
