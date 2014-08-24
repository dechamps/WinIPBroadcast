WinIPBroadcast changelog
========================

- 1.3 (24/08/2014)
  - Harden service security by removing most of its privileges. This prevents a potential attacker from gaining highly privileged access to the system by compromising WinIPBroadcast.
- 1.2 (15/11/2009)
  - Forgot to close a socket when sending broadcasts, which could cause serious problems on the system (e.g. total freezes) when sending large numbers of broadcast packets.
- 1.1 (31/10/2009)
  - Handle `WSAEHOSTUNREACH` correctly in `broadcastRouteAddress()`. Allows WinIPBroadcast to survive waking from sleep.
- 1.0 (21/10/2009)
  - First public release
