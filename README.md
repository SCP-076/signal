# SourceMod Signal-Slot Extension

**Requirements:** SourceMod 1.12+

This is a lightweight extension that introduces a **Declarative Event Bus** (or Pub/Sub system) for internal communication within a single SourceMod plugin. 

Instead of manually registering forwards or calling initialization functions across dozens of files, this extension utilizes a **pubvar-driven auto-wiring mechanism**. By using simple macros to create specifically formatted `pubvar`s, the extension scans these variables during the plugin's load phase and automatically wires up your internal event network.

This allows massive, monolithic `.smx` projects to be easily and cleanly decoupled, letting you focus on feature logic rather than boilerplate wiring.

---

## 1. Basic Usage: Decoupling Module Initialization

The most common use case is broadcasting a global initialization event to multiple independent modules without having to manually call each function in your main file.

```sourcepawn
#include <signal>

// 1. Create a signal named 'InitAllModules'
SIG(InitAllModules)

// --- File 1 ---
// 2. Declare the connection between 'Module1' and the 'InitAllModules' signal
CONNECT_SIG(Module1, InitAllModules)
public void Module1()
{
    // Module 1 initialization logic...
}

// --- File 2 ---
CONNECT_SIG(Module2, InitAllModules)
public void Module2()
{
    // Module 2 initialization logic...
}

// --- Main File ---
public void OnPluginStart()
{
    // 3. Emit the signal, executing both Module1 and Module2 initialization logic.
    // The ES_FailOnError flag ensures that if any module throws an error, 
    // it propagates outward to safely abort the plugin loading process.
    InitAllModules.Emit(_, ES_FailOnError);
}
```

A step further:
```sourcepawn
SIG(SignalPluginStart)

//Further abstraction
#define PluginStart(%1) CONNECT_SIG(OnPluginStart_%1,SignalPluginStart) public void OnPluginStart_%1()


PluginStart(ModuleA)
{

}

PluginStart(ModuleB)
{

}

PluginStart(ModuleC)
{

}

```

## 2. Payloads & Action Returns
Signals can pass a single argument to all connected slots. 

If slot functions return an `Action`, the emitter will aggregate them and return the highest `Action` value across all executions.

```sourcepawn
SIG(ClientSettingsChanged)

public void OnClientSettingsChanged(int client)
{
    // Emits the payload and captures the highest Action returned
    Action result = ClientSettingsChanged.Emit(client);
    if (result != Plugin_Continue)
    {
        KickClient(client);
    }
}

// --- Module A ---
CONNECT_SIG(FunctionA, ClientSettingsChanged)
public Action FunctionA(int client)
{
    return Plugin_Continue;
}

// --- Module B ---
CONNECT_SIG(FunctionB, ClientSettingsChanged)
public Action FunctionB(int client)
{
    // This will become the final result if it's the highest Action
    return Plugin_Handled;
}
```

## 3. Execution Priorities
By default, slots have a neutral priority (0). If you need a strict execution order, use the CONNECT_SIG_EX macro. Slots with higher priority values will always execute first.

```sourcepawn
CONNECT_SIG_EX(FuncA, AnySignal, SigPriority(100))  // Executes first
CONNECT_SIG_EX(FuncB, AnySignal, SigPriority(-10))  // Executes last
CONNECT_SIG(FuncC, AnySignal)                       // Default priority (0)
```
> **Debugging Tip:** Use the server console command `sm signals` to dump a list of active signal connections and their exact execution order.

---

## Important Notes & Limitations

* **Public Functions Only:** Signals can only be connected to `public` functions. `methodmap` methods are strictly unsupported.
* **Safety Checks:** The provided macros perform automatic type-checking during compilation and will throw syntax errors if signatures do not match. While the compiler cannot verify if a connected function is actually marked as `public`, the extension securely handles this by detecting invalid bindings during the load phase and preventing the plugin from running.


## Changelog

### v1.2.1
* **Enhanced Error Handling:** Switched to using SourceMod's internal `EvictWithError`.
* **New Lifecycle Signal:** Added the `OnAllSignalsLoaded` auto-triggered signal. This fires immediately after all signal-slot wiring is completed but *before* `OnPluginStart()`, making it the perfect injection point for early modular initialization in `.inc` files.
* **New Utilities (console_macro.inc, event_hook_macro.inc):** Shortcut macros using OnAllSignalsLoaded for declarative, anywhere-in-code command and event registration.

#### `console_macro.inc` Usage Example:
```sourcepawn
#include <console_macro> // Just include this to use the shortcut macros


// Example: Create a console command "sm_console1" from anywhere in your plugin
CONSOLE_COMMAND(sm_console1, "This is a console command")
{
    PrintToServer("%N used sm_console1 (args=%i)", client, args);
    return Plugin_Handled;
}


// Example: Create an admin command "sm_admin1" from anywhere in your plugin
ADMIN_COMMAND(sm_admin1, ADMFLAG_ROOT, "This is an admin command")
{
    PrintToServer("Admin %N used sm_admin1 (args=%i)", client, args);
    return Plugin_Handled;
}

```


#### `event_hook_macro.inc` Usage Example:
```sourcepawn
#include <event_hook_macro> 


ON_EVENT_PRE(player_say)
{
	int client = GetClientOfUserId(event.GetInt("userid"));

	PrintToServer("ON_EVENT_PRE: player_say (%N)",client);
}


ON_EVENT_POST(player_say)
{
	int client = GetClientOfUserId(event.GetInt("userid"));
	
	PrintToServer("ON_EVENT_POST: player_say (%N)",client);
}


```