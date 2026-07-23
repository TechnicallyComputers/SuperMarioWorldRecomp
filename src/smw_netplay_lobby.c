#include "smw_netplay_lobby.h"

#ifdef SMW_COOP_BUILD

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "snes_host_lobby.h"
#include "snes_lobby_client.h"

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
  out->input_delay = 2;
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

static uint64_t AutoNowMs(void) {
#ifdef _WIN32
  return (uint64_t)GetTickCount64();
#else
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
#endif
}

static void AutoSleepMs(unsigned ms) {
#ifdef _WIN32
  Sleep(ms);
#else
  struct timespec ts;
  ts.tv_sec = (time_t)(ms / 1000u);
  ts.tv_nsec = (long)(ms % 1000u) * 1000000L;
  nanosleep(&ts, NULL);
#endif
}

static int AutoTimedOut(uint64_t deadline) {
  return AutoNowMs() >= deadline;
}

static void AutoLog(const char *role, const char *stage) {
  const RecompLauncherCNetplayCallbacks *cb = snes_host_lobby_callbacks();
  const char *err = (cb && cb->last_error) ? cb->last_error(NULL) : "";
  fprintf(stderr, "[netplay selftest] role=%s stage=%s members=%d ready=%d "
                  "in_lobby=%d error=%s\n",
          role, stage, snes_lobby_member_count(), snes_lobby_all_ready(),
          snes_lobby_in_lobby(), err ? err : "");
}

int SmwNetplayLauncherAutoLaunch(const char *role, const char *player_name,
                                 const char *lobby_name,
                                 unsigned timeout_ms,
                                 RecompLauncherCNetplayLaunch *out) {
  const RecompLauncherCNetplayCallbacks *cb;
  RecompLauncherCSettings settings;
  RecompLauncherCNetplayLobby row;
  uint64_t deadline;
  uint64_t next_list = 0;
  int is_host;
  int joined = 0;
  int host_ready_sent = 0;
  int start_sent = 0;
  int i;

  SmwHostLobbyEnsureInit();
  cb = snes_host_lobby_callbacks();
  if (!cb) return -1;
  if (!role || !lobby_name || !lobby_name[0] || !out) return -1;
  is_host = strcmp(role, "host") == 0;
  if (!is_host && strcmp(role, "guest") != 0) return -1;
  if (timeout_ms < 1000u) timeout_ms = 60000u;
  deadline = AutoNowMs() + timeout_ms;
  memset(&settings, 0, sizeof(settings));
  memset(out, 0, sizeof(*out));
  cb->set_player_name(NULL, player_name && player_name[0] ? player_name
                                                          : (is_host ? "HostTest"
                                                                     : "GuestTest"));
  if (cb->connect(NULL) != 0) {
    AutoLog(role, "connect_failed");
    return -2;
  }

  while (!snes_lobby_player_id()[0]) {
    cb->pump(NULL);
    if (!cb->connected(NULL)) {
      AutoLog(role, "handshake_disconnected");
      return -3;
    }
    if (AutoTimedOut(deadline)) {
      AutoLog(role, "welcome_timeout");
      return -4;
    }
    AutoSleepMs(10);
  }
  AutoLog(role, "connected");

  if (is_host) {
    char endpoint[64] = "0.0.0.0:7777";
    if (cb->create(NULL, lobby_name, endpoint, "", &settings, 0) != 0) {
      AutoLog(role, "create_failed");
      return -5;
    }
  }

  while (!cb->launch_pending(NULL)) {
    cb->pump(NULL);
    if (!cb->connected(NULL)) {
      AutoLog(role, "lobby_disconnected");
      return -6;
    }

    if (!is_host && !joined && AutoNowMs() >= next_list) {
      cb->request_list(NULL);
      next_list = AutoNowMs() + 500u;
    }
    if (!is_host && !joined) {
      for (i = 0; i < cb->list_count(NULL); ++i) {
        if (cb->list_get(NULL, i, &row) &&
            strcmp(row.name, lobby_name) == 0 &&
            strcmp(row.game_name, SMW_NETPLAY_GAME) == 0) {
          char guest_bind[64];
          guest_bind[0] = '\0';
          if (cb->join(NULL, row.lobby_id, "", guest_bind) != 0) {
            AutoLog(role, "join_failed");
            return -7;
          }
          joined = 1;
          AutoLog(role, "join_sent");
          break;
        }
      }
    }

    if (is_host && cb->in_lobby(NULL) && cb->member_count(NULL) >= 2 &&
        !host_ready_sent) {
      if (cb->set_ready(NULL, 1) != 0) {
        AutoLog(role, "ready_failed");
        return -8;
      }
      host_ready_sent = 1;
      AutoLog(role, "ready_sent");
    }
    if (is_host && host_ready_sent && cb->all_ready(NULL) && !start_sent) {
      if (cb->request_start(NULL, &settings) != 0) {
        AutoLog(role, "start_failed");
        return -9;
      }
      start_sent = 1;
      AutoLog(role, "start_sent");
    }

    if (AutoTimedOut(deadline)) {
      AutoLog(role, "launch_timeout");
      return -10;
    }
    AutoSleepMs(10);
  }
  if (!cb->fill_launch(NULL, out)) {
    AutoLog(role, "invalid_launch");
    return -11;
  }
  fprintf(stderr,
          "[netplay selftest] role=%s stage=launch slot=%d session=%u "
          "bind=%s peer=%s\n",
          role, out->local_slot, (unsigned)out->session_id,
          out->bind_hostport, out->peer_hostport);
  return 0;
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
