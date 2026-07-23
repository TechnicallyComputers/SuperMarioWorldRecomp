#ifndef SMW_NETPLAY_LOBBY_H
#define SMW_NETPLAY_LOBBY_H

#ifdef SMW_COOP_BUILD

#include "recomp_launcher.h"

/* Thin SMW Co-op wrappers over snesrecomp snes_host_lobby_* (MotK + LAN).
 * Identity / match_caps / auto-ready live in smw_netplay_lobby.c; do not
 * grow a second lobby callback table here. */
const RecompLauncherCNetplayCallbacks *SmwNetplayLauncherCallbacks(void);
/* Deterministic integration-test path (no ImGui). Leaves the MotK socket
 * open after success so ICE signaling can continue. */
int SmwNetplayLauncherAutoLaunch(const char *role, const char *player_name,
                                 const char *lobby_name,
                                 unsigned timeout_ms,
                                 RecompLauncherCNetplayLaunch *out);
void SmwNetplayLauncherPrepareRematch(void);
const char *SmwNetplayLauncherResumeEndpoint(void);
void SmwNetplayLauncherSetRuntimeError(const char *error_code);
void SmwNetplayLauncherDisconnect(void);

#endif

#endif /* SMW_NETPLAY_LOBBY_H */
