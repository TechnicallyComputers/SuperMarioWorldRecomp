#include "smw_netplay_lobby.h"

#ifdef SMW_COOP_BUILD

#include <stdio.h>
#include <string.h>

#include "snes_host_lobby.h"

#define SMW_NETPLAY_GAME "Super Mario World Co-op"

static void SmwFillMatchCaps(void *ctx, const void *settings_v,
                             SnesLobbyMatchCaps *out) {
  const RecompLauncherCSettings *settings =
      (const RecompLauncherCSettings *)settings_v;
  (void)ctx;
  if (!out) return;
  out->valid = 1;
  out->widescreen = 0;
  out->widescreen_hud = 0;
  out->ignore_aspect = settings ? settings->ignore_aspect != 0 : 0;
  /* Preserve out->input_delay from default_caps (lobby waiting-room setting). */
  out->ws_extra = 0;
}

static void SmwHostLobbyEnsureInit(void) {
  static int once;
  SnesHostLobbyIdentity id;
  SnesHostLobbyOpts opts;
  if (once) return;
  once = 1;
  memset(&id, 0, sizeof(id));
  id.game_name = SMW_NETPLAY_GAME;
  id.game_version = SNES_GAME_VERSION;
  id.lan_registry_path = "netplay_lan_lobby.txt";
  id.default_lobby_name = "SMW Co-op Lobby";
  memset(&opts, 0, sizeof(opts));
  opts.auto_ready_guests = 1;
  opts.rematch_set_ready = 0;
  opts.fill_match_caps = &SmwFillMatchCaps;
  if (snes_host_lobby_init(&id, &opts) != 0)
    fprintf(stderr, "smw-coop: snes_host_lobby_init failed\n");
}

int SmwNetplayLauncherAutoLaunch(const char *role, const char *player_name,
                                 const char *lobby_name,
                                 unsigned timeout_ms,
                                 RecompLauncherCNetplayLaunch *out) {
  SmwHostLobbyEnsureInit();
  return snes_host_lobby_auto_launch(role, player_name, lobby_name, timeout_ms,
                                     out);
}

const RecompLauncherCNetplayCallbacks *SmwNetplayLauncherCallbacks(void) {
  SmwHostLobbyEnsureInit();
  return snes_host_lobby_callbacks();
}

void SmwNetplayLauncherPrepareRematch(void) {
  SmwHostLobbyEnsureInit();
  snes_host_lobby_prepare_rematch();
}

const char *SmwNetplayLauncherResumeEndpoint(void) {
  const char *ep;
  SmwHostLobbyEnsureInit();
  ep = snes_host_lobby_resume_endpoint();
  return (ep && ep[0]) ? ep : NULL;
}

void SmwNetplayLauncherSetRuntimeError(const char *error_code) {
  SmwHostLobbyEnsureInit();
  snes_host_lobby_set_runtime_error(error_code);
}

void SmwNetplayLauncherDisconnect(void) {
  SmwHostLobbyEnsureInit();
  snes_host_lobby_disconnect();
}

#endif /* SMW_COOP_BUILD */
