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
#include <IRootConsoleMenu.h>

using namespace SourceMod;
using namespace SourcePawn;

/**
 * @file extension.cpp
 * @brief Implement extension code here.
 */

Sample g_Sample;		/**< Global singleton for extension's main interface */

SMEXT_LINK(&g_Sample);

#define MAX_LEN_SIGNAL_NAME 32
#define MAX_LEN_SIGNAL_SLOT 32

// ======================================================================
// 1. Memory-mapped structure (Ensure strict alignment with inc)
// ======================================================================
struct Cpp_SignalSlot {
    cell_t global;       // Offset: 0 bytes
    cell_t priority;     // Offset: 4 bytes
    char slot[MAX_LEN_SIGNAL_SLOT];   // Offset: 8 bytes
    char signal[MAX_LEN_SIGNAL_NAME]; // Offset: 40 bytes
};
static_assert(sizeof(Cpp_SignalSlot) == 72, "Cpp_SignalSlot size mismatch! Check struct padding.");

// ======================================================================
// 2. Core Data Containers and Control Flags
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
// 3. Native Function Implementations
// ======================================================================
cell_t EmitSignal_Native(IPluginContext* pContext, const cell_t* params)
{
    // params[1] = signal (Local array address of the Signal)
    // params[2] = data
    // params[3] = flags (EmitSignalFlag)

    int flags = params[3];

    // Convert the provided local signal address to the plugin's physical memory address
    cell_t* phys_addr;
    if (pContext->LocalToPhysAddr(params[1], &phys_addr) != SP_ERROR_NONE) {
        return pContext->ThrowNativeError("Invalid signal address provided.");
    }

    // Look up the signal container for the current plugin
    auto it = g_PluginSignals.find(pContext);
    if (it == g_PluginSignals.end()) {
        if (flags & ES_RequireSlot) {
            return pContext->ThrowNativeError("ES_RequireSlot: no signal context initialized in this plugin.");
        }
        return 0;
    }

    // O(1) fast matching using physical address
    auto sig_it = it->second.signals.find(phys_addr);

    if (sig_it == it->second.signals.end() || sig_it->second.slots.empty()) {
        if (flags & ES_RequireSlot) {
            return pContext->ThrowNativeError("ES_RequireSlot: cannot find any slot function for the given signal.");
        }
        return 0;
    }

    cell_t max_result = 0;
    bool has_executed = false;
    cell_t data = params[2];

    for (const SlotInfo& slot : sig_it->second.slots)
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

            if ((flags & ES_StopOnHandled) && max_result >= 3) {
                break;
            }
        }
        else
        {
            if (flags & ES_FailOnError) {
                return pContext->ThrowNativeError("ES_FailOnError: stop emit signal '%s'.", sig_it->second.signal_name.c_str());
            }
        }
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

    // Look up the signal container for the current plugin
    auto it = g_PluginSignals.find(pContext);
    if (it == g_PluginSignals.end()) {
        return 0;
    }

    // O(1) match physical address to find the slot list for this signal
    auto sig_it = it->second.signals.find(phys_addr);
    if (sig_it == it->second.signals.end()) {
        return 0;
    }

    return static_cast<cell_t>(sig_it->second.slots.size());
}

// ======================================================================
// 4. Plugin Lifecycle Manager (Parsing, Validation, and Interception)
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
            return; // Skip if the activation identifier is not present
        }

        PluginSignalContainer container;
        container.plugin_name = plugin->GetFilename();
        sp_pubvar_t* pubvar;

        for (uint32_t i = 0; context->GetPubvarByIndex(i, &pubvar) == SP_ERROR_NONE; i++)
        {
            if (strncmp(pubvar->name, "__CSIG_", 7) == 0)
            {
                Cpp_SignalSlot* slot_data = reinterpret_cast<Cpp_SignalSlot*>(pubvar->offs);

                // Extract safe strings for lookup and error reporting
                std::string safe_slot_name(slot_data->slot, strnlen(slot_data->slot, MAX_LEN_SIGNAL_SLOT));
                std::string safe_signal_name(slot_data->signal, strnlen(slot_data->signal, MAX_LEN_SIGNAL_NAME));

                // 1. Retrieve and verify the existence of the slot function
                IPluginFunction* func = context->GetFunctionByName(safe_slot_name.c_str());
                if (!func)
                {
                    context->ReportFatalError(
                        "'%s' failed to find function '%s' (connect signal: '%s'). Need 'public' modifier!",
                        plugin->GetFilename(),
                        safe_slot_name.c_str(),
                        safe_signal_name.c_str()
                    );
                    UnloadTargetPlugin(plugin);
                    return;
                }

                // 2. Get the physical address of the target signal based on the recorded signal string
                uint32_t sig_pubvar_idx;
                if (context->FindPubvarByName(safe_signal_name.c_str(), &sig_pubvar_idx) != SP_ERROR_NONE)
                {
                    context->ReportFatalError(
                        "'%s' failed to find signal '%s'. Ensure SIG(%s) is defined!",
                        plugin->GetFilename(),
                        safe_signal_name.c_str(),
                        safe_signal_name.c_str()
                    );
                    UnloadTargetPlugin(plugin);
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
                    SignalGroup group;
                    group.signal_name = safe_signal_name;
                    group.slots.push_back({ func, slot_data->priority });
                    container.signals[sig_phys_addr] = std::move(group);
                }
            }
        }

        // Sort by priority
        for (auto& pair : container.signals) {
            std::sort(pair.second.slots.begin(), pair.second.slots.end());
        }

        if (!container.signals.empty()) {
            g_PluginSignals[context] = std::move(container);
        }
    }

    void OnPluginDestroyed(IPlugin* plugin) override
    {
        if (plugin->GetBaseContext()) {
            g_PluginSignals.erase(plugin->GetBaseContext());
        }
    }

private:
    void UnloadTargetPlugin(IPlugin* plugin)
    {
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "sm plugins unload %s\n", plugin->GetFilename());
        gamehelpers->ServerCommand(cmd);
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
                rootconsole->ConsolePrint("  Signal: %s (Address: %p)", sig_pair.second.signal_name.c_str(), sig_pair.first);

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
// 5. Registration List and Lifecycle
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

// ======================================================================
// 6. SourceMod Extension Lifecycle Callbacks
// ======================================================================

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