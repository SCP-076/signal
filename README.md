# SourceMod Static Signal-Slot Extension

**Requirements:** SourceMod 1.12+

This extension implements a **Static Registration Pattern** for **internal communication within a single SourceMod plugin**. By scanning specific `pubvar`s during the plugin's load phase, it automatically wires an internal event network. This allows a large, monolithic `.smx` project—even one compiled from dozens or hundreds of source files—to be easily and cleanly decoupled.

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

## 2. Passing Parameters & Handling Action Returns
Signals can pass a single parameter payload and connect to slot functions that return an Action. When emitted, the signal will return the highest Action value across all executed slots.

```sourcepawn
SIG(ClientSettingsChanged)

public void OnClientSettingsChanged(int client)
{
    Action result = ClientSettingsChanged.Emit(client);
    if (result != Plugin_Continue)
    {
        KickClient(client);
    }
}

// --- Slot A ---
CONNECT_SIG(FunctionA, ClientSettingsChanged)
public Action FunctionA(int client)
{
    // Do some checks
    return Plugin_Continue;
}

// --- Slot B ---
CONNECT_SIG(FunctionB, ClientSettingsChanged)
public Action FunctionB(int client)
{
    // Do some checks
    return Plugin_Handled;
}
```

## 3. Execution Priorities
By default, the execution order of slot functions is not guaranteed. If you require strict execution sequencing, use the CONNECT_SIG_EX macro to assign a custom priority. Higher priority values execute first.

```sourcepawn
CONNECT_SIG_EX(FuncA, AnySignal, SigPriority(100))  // Executes first (highest priority)
CONNECT_SIG_EX(FuncB, AnySignal, SigPriority(-10))  // Priority can be a negative value
CONNECT_SIG(FuncC, AnySignal)                       // Default macro assigns a priority of 0
```
> **Debugging Tip:** Use the server console command `sm signals` to dump a list of active signal connections and view the exact execution order of all slot functions per plugin.

---

## Important Notes & Limitations

* **`public` Functions Only:** Signals can only be connected to `public` functions. `methodmap` methods are strictly unsupported.
* **Safety Checks:** The provided macros perform automatic type-checking during compilation and will throw syntax errors if signatures do not match. While the compiler cannot verify if a connected function is actually marked as `public`, the extension securely handles this by detecting invalid bindings during the load phase and preventing the plugin from running.