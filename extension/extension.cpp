/**
 * vim: set ts=4 :
 * =============================================================================
 * SourceMod Sample Extension
 * Copyright (C) 2004-2008 AlliedModders LLC.  All rights reserved.
 * =============================================================================
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License, version 3.0, as published by the
 * Free Software Foundation.
 * * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * As a special exception, AlliedModders LLC gives you permission to link the
 * code of this program (as well as its derivative works) to "Half-Life 2," the
 * "Source Engine," the "SourcePawn JIT," and any Game MODs that run on software
 * by the Valve Corporation.  You must obey the GNU General Public License in
 * all respects for all other code used.  Additionally, AlliedModders LLC grants
 * this exception to all derivative works.  AlliedModders LLC defines further
 * exceptions, found in LICENSE.txt (as of this writing, version JULY-31-2007),
 * or <http://www.sourcemod.net/license.php>.
 *
 * Version: $Id$
 */

#include "extension.h"

#include <unordered_map>
#include <vector>
#include <string>
#include <algorithm>
#include <cstring>
#include <cctype>
#include <IRootConsoleMenu.h>

using namespace SourceMod;
using namespace SourcePawn;

/**
 * @file extension.cpp
 * @brief Implement extension code here.
 */

Sample g_Sample;		/**< Global singleton for extension's main interface */

SMEXT_LINK(&g_Sample);

// ======================================================================
// [WARNING] DANGEROUS VTABLE HACK 
// ======================================================================
// The following SMPlugin interface is an internal SourceMod class copied 
// from bridge/include/IScriptManager.h to access the EvictWithError method.
// 
// If SourceMod ever changes the virtual function layout (vtable) of SMPlugin 
// in a future update (e.g., adding/removing virtual functions above EvictWithError), 
// this extension WILL CAUSE SERVER CRASHES and must be updated to match the new layout!
// ======================================================================
class AutoConfig;

class SMPlugin : public SourceMod::IPlugin
{
public:
    virtual size_t GetConfigCount() = 0;
    virtual AutoConfig* GetConfig(size_t i) = 0;
    virtual void AddLibrary(const char* name) = 0;
    virtual void AddConfig(bool create, const char* cfg, const char* folder) = 0;
    virtual void EvictWithError(SourceMod::PluginStatus status, const char* fmt, ...) = 0;
};



// ======================================================================
// ======================================================================
#define MAX_LEN_SIGNAL_NAME 32
#define MAX_LEN_SIGNAL_SLOT 32

// ======================================================================
//  Memory-mapped structure (Ensure strict alignment with inc)
// ======================================================================
struct Cpp_SignalSlot {
    cell_t global;       // Offset: 0 bytes
    cell_t priority;     // Offset: 4 bytes
    char slot[MAX_LEN_SIGNAL_SLOT];   // Offset: 8 bytes
    char signal[MAX_LEN_SIGNAL_NAME]; // Offset: 40 bytes
};
static_assert(sizeof(Cpp_SignalSlot) == 72, "Cpp_SignalSlot size mismatch! Check struct padding.");



// ======================================================================
//  Core Data Containers and Control Flags
// ======================================================================
struct SlotInfo {
    IPluginFunction* func;
    int priority;

    bool operator<(const SlotInfo& other) const {
        return priority > other.priority;
    }
};

struct SignalGroup {
    std::string signal_name;
    std::vector<SlotInfo> slots;
    bool is_private = false; // Flag for extension-exclusive signals (e.g., OnAllSignalsLoaded)
};

struct PluginSignalContainer {
    std::string plugin_name;
    std::unordered_map<cell_t*, SignalGroup> signals;
};

std::unordered_map<IPluginContext*, PluginSignalContainer> g_PluginSignals;

enum EmitSignalFlag
{
    ES_None = 0,
    ES_RequireSlot = (1 << 0),
    ES_FailOnError = (1 << 1),
    ES_StopOnHandled = (1 << 2)
};

// ======================================================================
// ======================================================================

static void TrimWhitespace(std::string& str) {
    // Trim left
    str.erase(str.begin(), std::find_if(str.begin(), str.end(), [](unsigned char ch) {
        return !std::isspace(ch);
        }));
    // Trim right
    str.erase(std::find_if(str.rbegin(), str.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
        }).base(), str.end());
}


// Returns SP_ERROR_NONE (0) on full success. 
// If internal_abort_on_error is true, or ES_FailOnError is set, it will 
// return the error code immediately upon the first failing slot.
static int ExecuteSignalGroup(const SignalGroup& group, cell_t data, int flags, cell_t& max_result, bool internal_abort_on_error = false)
{
    max_result = 0;
    bool has_executed = false;
    bool stop_on_handled = (flags & ES_StopOnHandled) != 0;
    bool native_fail_on_error = (flags & ES_FailOnError) != 0;

    for (const SlotInfo& slot : group.slots)
    {
        slot.func->PushCell(data);

        cell_t result = 0;
        int err = slot.func->Execute(&result);

        if (err == SP_ERROR_NONE)
        {
            if (!has_executed || result > max_result) {
                max_result = result;
                has_executed = true;
            }

            if (stop_on_handled && max_result >= 3) {
                break;
            }
        }
        else
        {
            // SourceMod VM automatically logs the stack trace to the server console.
            // We just need to decide whether to abort the remaining slots loop.
            if (internal_abort_on_error || native_fail_on_error) {
                return err; // Silent fail at the C++ loop level, let caller handle reporting.
            }
        }
    }
    return SP_ERROR_NONE;
}

// ======================================================================
//  Natives
// ======================================================================
cell_t EmitSignal_Native(IPluginContext* pContext, const cell_t* params)
{
    // params[1] = signal (Local array address of the Signal)
    // params[2] = data
    // params[3] = flags (EmitSignalFlag)

    int flags = params[3];

    cell_t* phys_addr;
    if (pContext->LocalToPhysAddr(params[1], &phys_addr) != SP_ERROR_NONE) {
        return pContext->ThrowNativeError("Invalid signal address provided.");
    }

    auto it = g_PluginSignals.find(pContext);
    if (it == g_PluginSignals.end()) {
        if (flags & ES_RequireSlot) {
            return pContext->ThrowNativeError("ES_RequireSlot: no signal context initialized in this plugin.");
        }
        return 0;
    }


    auto sig_it = it->second.signals.find(phys_addr);

    if (sig_it == it->second.signals.end() || sig_it->second.slots.empty()) {
        if (flags & ES_RequireSlot) {
            return pContext->ThrowNativeError("ES_RequireSlot: cannot find any slot function for the given signal.");
        }
        return 0;
    }

    if (sig_it->second.is_private) {
        return pContext->ThrowNativeError("Permission denied: Signal '%s' is restricted and can only be emitted internally by the extension.", sig_it->second.signal_name.c_str());
    }

    cell_t max_result = 0;
    cell_t data = params[2];

    // Standard Native Emit: Do not force internal abort, rely solely on Pawn flags.
    int err = ExecuteSignalGroup(sig_it->second, data, flags, max_result, false);

    // Only throw Native Error if the pawn-level ES_FailOnError flag was explicitly requested
    if (err != SP_ERROR_NONE && (flags & ES_FailOnError)) {
        return pContext->ThrowNativeError("ES_FailOnError: stop emit signal '%s'.", sig_it->second.signal_name.c_str());
    }

    return max_result;
}


cell_t GetSignalSlotCount_Native(IPluginContext* pContext, const cell_t* params)
{
    // params[1] = signal (Local array address of the Signal)

    cell_t* phys_addr;
    if (pContext->LocalToPhysAddr(params[1], &phys_addr) != SP_ERROR_NONE) {
        return pContext->ThrowNativeError("Invalid signal address provided.");
    }


    auto it = g_PluginSignals.find(pContext);
    if (it == g_PluginSignals.end()) {
        return 0;
    }


    auto sig_it = it->second.signals.find(phys_addr);
    if (sig_it == it->second.signals.end()) {
        return 0;
    }

    return static_cast<cell_t>(sig_it->second.slots.size());
}

// ======================================================================
//  Plugin Lifecycle Manager (Parsing, Validation, and Interception)
// ======================================================================
class SignalSlotManager : public IPluginsListener
{
public:
    void OnPluginLoaded(IPlugin* plugin) override
    {
        IPluginContext* context = plugin->GetBaseContext();
        if (!context) return;

        // Ensure no residual data remains
        g_PluginSignals.erase(context);

        uint32_t check_idx;
        if (context->FindPubvarByName("__include_signal__", &check_idx) != SP_ERROR_NONE) {
            return;
        }

        PluginSignalContainer container;
        container.plugin_name = plugin->GetFilename();
        sp_pubvar_t* pubvar;

        cell_t* on_all_loaded_addr = nullptr;

        for (uint32_t i = 0; context->GetPubvarByIndex(i, &pubvar) == SP_ERROR_NONE; i++)
        {
            if (strncmp(pubvar->name, "__CSIG_", 7) == 0)
            {
                Cpp_SignalSlot* slot_data = reinterpret_cast<Cpp_SignalSlot*>(pubvar->offs);

                // Extract safe strings for lookup and error reporting
                std::string safe_slot_name(slot_data->slot, strnlen(slot_data->slot, MAX_LEN_SIGNAL_SLOT));
                std::string safe_signal_name(slot_data->signal, strnlen(slot_data->signal, MAX_LEN_SIGNAL_NAME));

                // Trim any accidental whitespace introduced by macro stringification
                TrimWhitespace(safe_slot_name);
                TrimWhitespace(safe_signal_name);

                // 1. Retrieve and verify the existence of the slot function
                IPluginFunction* func = context->GetFunctionByName(safe_slot_name.c_str());
                if (!func)
                {
                    SMPlugin* sm_plugin = reinterpret_cast<SMPlugin*>(plugin);
                    sm_plugin->EvictWithError(Plugin_Error,
                        "'%s' failed to find public function '%s'.(Need add 'public' modifier!)",
                        plugin->GetFilename(),
                        safe_slot_name.c_str()
                    );
                    return;
                }

                // 2. Get the physical address of the target signal based on the recorded signal string
                uint32_t sig_pubvar_idx;
                if (context->FindPubvarByName(safe_signal_name.c_str(), &sig_pubvar_idx) != SP_ERROR_NONE)
                {
                    SMPlugin* sm_plugin = reinterpret_cast<SMPlugin*>(plugin);
                    sm_plugin->EvictWithError(Plugin_Error,
                        "'%s' failed to find signal '%s'. Ensure SIG(%s) is defined!",
                        plugin->GetFilename(),
                        safe_signal_name.c_str(),
                        safe_signal_name.c_str()
                    );
                    return;
                }

                sp_pubvar_t* sig_pubvar;
                context->GetPubvarByIndex(sig_pubvar_idx, &sig_pubvar);
                cell_t* sig_phys_addr = sig_pubvar->offs;

          
                // 3. Group slot function information by physical address
                auto sig_it = container.signals.find(sig_phys_addr);
                if (sig_it != container.signals.end())
                {
                    sig_it->second.slots.push_back({ func, slot_data->priority });
                }
                else
                {
                    if (safe_signal_name == "OnAllSignalsLoaded") {
                        on_all_loaded_addr = sig_phys_addr;
                    }

                    SignalGroup group;
                    group.signal_name = safe_signal_name;
                    group.is_private = (safe_signal_name == "OnAllSignalsLoaded");
                    group.slots.push_back({ func, slot_data->priority });
                    container.signals[sig_phys_addr] = std::move(group);
                }
            }
        }

        // Sort by priority
        for (auto& pair : container.signals) {
            std::sort(pair.second.slots.begin(), pair.second.slots.end());
        }

        if (container.signals.empty())
        {
            return;
        }

        g_PluginSignals[context] = std::move(container);
        // Fire the OnAllSignalsLoaded signal automatically if connected
        if (on_all_loaded_addr != nullptr) {
            auto sig_it = g_PluginSignals[context].signals.find(on_all_loaded_addr);
            if (sig_it != g_PluginSignals[context].signals.end()) {
                cell_t dummy_result = 0;

                // Use ES_None for standard flags, but set internal_abort_on_error = true
                // This creates a "silent fail" at the ExecuteSignalGroup level that we handle below.
                int err = ExecuteSignalGroup(sig_it->second, 0, ES_None, dummy_result, true);

                if (err != SP_ERROR_NONE) {
                    SMPlugin* sm_plugin = reinterpret_cast<SMPlugin*>(plugin);
                    sm_plugin->EvictWithError(Plugin_Error,
                        "'%s' failed during initialization: Error executing slot for signal '%s'.",
                        plugin->GetFilename(),
                        sig_it->second.signal_name.c_str()
                    );

                    g_PluginSignals.erase(context);
                    return;
                }
            }
        }
    }

    // Clear the signal cache as late as possible to prevent any lingering forwards in the plugin from using signals.
    void OnPluginDestroyed(IPlugin* plugin) override
    {
        if (plugin->GetBaseContext()) {
            g_PluginSignals.erase(plugin->GetBaseContext());
        }
    }
};

SignalSlotManager g_SignalSlotManager;

// ======================================================================
// Console Command: sm signals
// ======================================================================

class DumpSignalCommand : public IRootConsoleCommand
{
public:
    void OnRootConsoleCommand(const char* cmdname, const ICommandArgs* command) override
    {
        const char* target_plugin = nullptr;
        if (command->ArgC() >= 3) {
            target_plugin = command->Arg(2);
        }

        if (target_plugin) {
            rootconsole->ConsolePrint("Dumping active signals and slots in execution order for plugin: %s", target_plugin);
        }
        else {
            rootconsole->ConsolePrint("Dumping active signals and slots in execution order:");
        }

        if (g_PluginSignals.empty()) {
            rootconsole->ConsolePrint("  No active signals found.");
            return;
        }

        bool found_any = false;

        for (const auto& plugin_pair : g_PluginSignals) {
            std::string current_plugin_name = plugin_pair.second.plugin_name;

            if (target_plugin != nullptr) {
                if (current_plugin_name.find(target_plugin) == std::string::npos) {
                    continue;
                }
            }

            found_any = true;
            rootconsole->ConsolePrint("--------------------------------------------------------");
            rootconsole->ConsolePrint("Plugin: %s", current_plugin_name.c_str());

            for (const auto& sig_pair : plugin_pair.second.signals) {
                rootconsole->ConsolePrint("  Signal: %s (Address: %p)%s",
                    sig_pair.second.signal_name.c_str(),
                    sig_pair.first,
                    sig_pair.second.is_private ? " [Private]" : "");

                for (const auto& slot : sig_pair.second.slots) {
                    rootconsole->ConsolePrint("    -> Slot: %s [Priority: %d]", slot.func->DebugName(), slot.priority);
                }
            }
        }

        if (target_plugin != nullptr && !found_any) {
            rootconsole->ConsolePrint("  No active signals found for plugin matching '%s'.", target_plugin);
        }
    }
}g_DumpSignalCmd;

// ======================================================================
//  Registration and Lifecycle
// ======================================================================

static const sp_nativeinfo_t MyNatives[] =
{
    {"EmitSignal", EmitSignal_Native},
    {"GetSignalSlotCount", GetSignalSlotCount_Native},
    {NULL,         NULL},
};

void InitializeSignalSlotSystem(bool late)
{
    plsys->AddPluginsListener(&g_SignalSlotManager);
    rootconsole->AddRootConsoleCommand3("signals", "Dump all active signal slots", &g_DumpSignalCmd);
    if (late) {
        IPluginIterator* iter = plsys->GetPluginIterator();
        while (iter->MorePlugins()) {
            IPlugin* plugin = iter->GetPlugin();
            if (plugin->GetStatus() == Plugin_Running) {
                g_SignalSlotManager.OnPluginLoaded(plugin);
            }
            iter->NextPlugin();
        }
        iter->Release();
    }
}

void ShutdownSignalSlotSystem()
{
    rootconsole->RemoveRootConsoleCommand("signals", &g_DumpSignalCmd);
    plsys->RemovePluginsListener(&g_SignalSlotManager);
    g_PluginSignals.clear();
}


bool Sample::SDK_OnLoad(char* error, size_t maxlength, bool late)
{
    InitializeSignalSlotSystem(late);
    return true;
}

void Sample::SDK_OnUnload()
{
    ShutdownSignalSlotSystem();
}

void Sample::SDK_OnAllLoaded()
{
    sharesys->AddNatives(myself, MyNatives);
}