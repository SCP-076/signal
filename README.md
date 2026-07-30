# SourceMod Signal-Slot Extension

**Requirements:** SourceMod 1.12+

A lightweight extension that brings a **declarative Pub/Sub system** to SourcePawn, making it easy to decouple large single-plugin (`.smx`) projects. Define signals and wire up slots with simple macros—the extension handles the rest automatically at load time.

---

## 1. Basic Usage

```sourcepawn
#include <signal>

// 1. Define a signal
SIG(InitAllModules)

// 2. Connect a slot function from anywhere in your code
CONNECT_SIG(Module1, InitAllModules)
public void Module1()
{
    // Module 1 init logic...
}

// 3. Emit the signal to trigger all connected slots
public void OnPluginStart()
{
    InitAllModules.Emit(_, ES_FailOnError);
}
```

---

## 2. Payloads & Action Returns

Signals pass a single argument to all slots. Slots returning `Action` can influence the emitter.

```sourcepawn
SIG(ClientSettingsChanged)

public void OnClientSettingsChanged(int client)
{
    Action result = ClientSettingsChanged.Emit(client);
    if (result != Plugin_Continue)
        KickClient(client);
}

CONNECT_SIG(CheckBanned, ClientSettingsChanged)
public Action CheckBanned(int client)
{
    return Plugin_Continue;
}

CONNECT_SIG(EnforceRestrictions, ClientSettingsChanged)
public Action EnforceRestrictions(int client)
{
    return Plugin_Handled; // highest Action wins
}
```

---

## 3. Execution Priorities

```sourcepawn
CONNECT_SIG_EX(FuncA, AnySignal, SigPriority(100))  // executes first
CONNECT_SIG_EX(FuncB, AnySignal, SigPriority(-10))  // executes last
CONNECT_SIG(FuncC, AnySignal)                       // default priority (0)
```

> **Debugging:** Run `sm signals` in the server console to inspect all active signals and their execution order.

---

## 4. AutoCvar — Bind ConVar values to global variables

`auto_cvar.inc` wraps SourceMod ConVars so assigned globals stay **automatically in sync** with the latest cvar value—no manual `GetConVar*` calls needed.

### Bind an existing cvar
```sourcepawn
#include <auto_cvar>

CVAR_INT   (g_iHostport,   BindConVar("hostport"))
CVAR_STRING(g_sHostname,64,BindConVar("hostname"))

void PrintInfo()
{
    PrintToServer("port=%i  hostname=%s", g_iHostport, g_sHostname);
}
```

### Create a plugin-owned cvar
```sourcepawn
CVAR_INT   (g_iMyCvar,    CreateConVar("sm_my_int",   "123"))
CVAR_FLOAT (g_fMyCvar,    CreateConVar("sm_my_float", "3.14").Bounds(0.0, 10.0))
CVAR_BOOL  (g_bMyCvar,    CreateConVar("sm_my_bool",  "1"))
CVAR_STRING(g_sMyCvar, 32,CreateConVar("sm_my_str",   "Hello"))
```

### Chain a change callback
```sourcepawn
CVAR_BOOL(g_bCheats, BindConVar("sv_cheats").Callback(OnCheatsChanged))

void OnCheatsChanged(AutoCvar cvar, const char[] oldVal, const char[] newVal)
{
    if (g_bCheats)
        PrintToServer("cheats enabled");
}
```

### Available chain options
| Type | `.Callback` | `.Min` | `.Max` | `.Bounds` |
|------|:--:|:--:|:--:|:--:|
| `CVAR_INT` | ✓ | ✓ | ✓ | ✓ |
| `CVAR_FLOAT` | ✓ | ✓ | ✓ | ✓ |
| `CVAR_BOOL` | ✓ | — | — | — (enforced [0,1]) |
| `CVAR_STRING` | ✓ | — | — | — |

### Set values via the AutoCvar object
```sourcepawn
GetAutoCvar(g_iMyCvar).IntValue   += 1;
GetAutoCvar(g_fMyCvar).FloatValue = 2.5;
GetAutoCvar(g_bMyCvar).BoolValue   = false;
GetAutoStringCvar(g_sMyCvar).SetString("new value");
```

> See `example/test_auto_cvar.sp` for a full walkthrough.

---

## 5. Console & Event Macros

### `console_macro.inc` — declarative command registration

```sourcepawn
#include <console_macro>

CONSOLE_COMMAND(sm_console1, "A console command")
{
    PrintToServer("%N used sm_console1 (args=%i)", client, args);
    return Plugin_Handled;
}

ADMIN_COMMAND(sm_admin1, ADMFLAG_ROOT, "An admin command")
{
    PrintToServer("Admin %N used sm_admin1 (args=%i)", client, args);
    return Plugin_Handled;
}
```

### `event_hook_macro.inc` — declarative event hooking

```sourcepawn
#include <event_hook_macro>

ON_EVENT_PRE(player_say)
{
    int client = GetClientOfUserId(event.GetInt("userid"));
    PrintToServer("PRE player_say: %N", client);
}

ON_EVENT_POST(player_say)
{
    int client = GetClientOfUserId(event.GetInt("userid"));
    PrintToServer("POST player_say: %N", client);
}
```

---

## Important Notes

* **Public functions only.** `methodmap` methods are not supported as slots.
* Macros perform compile-time type checking. The extension catches missing `public` modifiers at load time and prevents the plugin from running.

---

## Changelog

### v1.5.0
* **AutoCvar (`auto_cvar.inc`):** Declarative ConVar wrapper that keeps typed global variables in sync automatically. Supports `BindConVar` (existing cvars) and `CreateConVar` (new plugin-owned cvars), with optional bounds chaining and change callbacks.

### v1.2.1
* Switched to SourceMod's internal `EvictWithError` for safer error handling.
* Added `OnAllSignalsLoaded` lifecycle signal.
* Added `console_macro.inc` and `event_hook_macro.inc` shortcut macros.