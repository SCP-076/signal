

#if defined _plugin_signalslot
	#endinput
#endif
#define _plugin_signalslot

/**
 * ============================================================================
 * Internal Signal-Slot System
 * Event bus for triggering module components within a single plugin.
 * ============================================================================
 */
	
enum EmitSignalFlag
{
	ES_None			 = 0,
	ES_RequireSlot	 = (1 << 0),	// Throws an exception directly if no callable slot functions are found.
	ES_FailOnError	 = (1 << 1),	// Throws an exception and aborts execution if any slot function encounters an error.
	ES_StopOnHandled = (1 << 2),	// Terminates execution early if any slot returns Plugin_Handled or Plugin_Stop.
}

typeset FunctionSignalSlot
{
	function void();
	function void(any data);
	function Action();
	function Action(any data);
}

//Declares a new signal variable.
#define SIG(%1) public const Signal %1;
#define SigPriority(%1) (%1)

/**
 * @brief Connects a slot function to a signal using the default priority (0).
 * 
 * @param %1        The slot function(public).
 * @param %2        The signal.
 */
#define CONNECT_SIG(%1,%2) CONNECT_SIG_EX(%1,%2,SigPriority(0))

/**
 * @brief Connects a slot function to a signal using a custom priority.
 * @param %1        The slot function(public).
 * @param %2        The signal.
 * @param %3        Custom priority value (higher values execute first).
 */
#define CONNECT_SIG_EX(%1,%2,%3) public const SignalSlotInfo __CSIG_%1_%2 = {false,%3,#%1,#%2};stock void __testslot_%1_%2(){Signal a; a = %2;FunctionSignalSlot f = %1;if(a._m && f){}}


enum struct Signal
{
	int _m;


	/**
	 * Emits this signal, triggering all connected slot functions.
	 * 
	 * @param data      Optional payload data to pass to the slots.
	 * @param flags     Bitmask of EmitSignalFlag defining execution behavior.
	 *
	 * @return          The highest Action returned by all executed slot functions 
	 *                  (returns plugin_continue if no values were returned).
	 *
	 * @error           Throws a native error in the following cases:
	 *                  - The ES_RequireSlot flag is set, but no valid slot functions
	 *                    are connected to this signal.
	 *                  - The ES_FailOnError flag is set, and a runtime error occurs 
	 *                    during the execution of any slot function.
	 */
    Action Emit(any data = 0,EmitSignalFlag flags = ES_None)
    {
        return EmitSignal(this,data,flags);
    }


	/**
	 * Retrieves the number of slot functions currently connected to this signal.
	 * 
	 * @return          The total count of active slots connected to the signal. 
	 *                  Returns 0 if none are connected.
	 */
    int SlotCount()
    {
        return GetSignalSlotCount(this);
    }
}

/**
 * Emits a signal, triggering all connected slot functions.
 *
 * @param signal    The signal variable to emit.
 * @param data      Optional payload data to pass to the slots.
 * @param flags     Bitmask of EmitSignalFlag defining execution behavior.
 *
 * @return          The highest Action returned by all executed slot functions 
 *                  (returns plugin_continue if no values were returned).
 *
 * @error           Throws a native error in the following cases:
 *                  - The ES_RequireSlot flag is set, but no valid slot functions
 *                    are connected to this signal.
 *                  - The ES_FailOnError flag is set, and a runtime error occurs 
 *                    during the execution of any slot function.
 */
native Action EmitSignal(Signal signal,any data = 0, EmitSignalFlag flags = ES_None);

/**
 * Retrieves the number of slot functions currently connected to a specific signal.
 *
 * @param signal    The signal variable to query.
 * @return          The total count of active slots connected to the signal. 
 *                  Returns 0 if none are connected.
 */
native int GetSignalSlotCount(Signal signal);



/**
 * Do not edit below this line!
 */

public Extension __ext_signal =
{
	name = "Signal Extension",
	file = "signal.ext",
#if defined AUTOLOAD_EXTENSIONS
	autoload = 1,
#else
	autoload = 0,
#endif
#if defined REQUIRE_EXTENSIONS
	required = 1,
#else
	required = 0,
#endif
};

enum struct SignalSlotInfo
{
	bool global;		
	int	 priority;		// Execution priority (higher values execute first).
	char slot[32];		// Name of the slot function.
	char signal[32];	// Name of the emitted signal.
}

// Ensure the structure size and offsets match the C++ extension layer perfectly.
static_assert(SignalSlotInfo::global == 0, "Member offset mismatch: global");
static_assert(SignalSlotInfo::priority * 4 == 4, "Member offset mismatch: priority");
static_assert(SignalSlotInfo::slot * 4 == 8, "Member offset mismatch: slot");
static_assert(SignalSlotInfo::signal * 4 == 40, "Member offset mismatch: signal");
static_assert(sizeof(SignalSlotInfo) * 4 == 72, "SignalSlotInfo size mismatch!");
public const int __include_signal__;

