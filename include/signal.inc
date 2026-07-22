

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


enum LocalAddr
{
	INVALID_LOCAL_ADDR = -1,
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
#define CONNECT_SIG_EX(%1,%2,%3) \
	public const SignalSlotInfo __CSIG_%1_%2 = {false,%3,#%1,#%2};\
	stock void __TEST_%1_%2(){\
		Signal a;a = %2;\
		FunctionSignalSlot f = %1;\
		if(a._dummy && f){}\
	}



enum struct Signal
{
	int _dummy;

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
 * Emitted automatically after all signals and slots within the plugin have 
 * been loaded.
 *
 * This signal triggers earlier than OnPluginStart(). At this execution stage, 
 * some core SourceMod and module natives are available to be called. However, 
 * many other subsystems (such as global forwards) are not guaranteed to be 
 * fully initialized yet.
 *
 * It is primarily intended for modular include files (.inc) to perform very 
 * limited, early setup operations before the main OnPluginStart() routine 
 * executes (e.g., initializing internal variables or offsets based on the 
 * specific game).
 *
 * @note            This is a restricted, private signal emitted internally 
 * by the extension. Manual emission is prohibited.
 *
 * @note            If any run-time error is thrown during this callback, 
 * the plugin will be marked as failed.
 */
SIG(OnAllSignalsLoaded)



/**
 * Gets the local-address offset of a cell variable.
 * 
 * @note            Should only be used on global or local-static variables;
 *                  stack-local addresses are invalid once the frame exits.
 * @param cell      Variable to query (by-ref).
 * @return          LocalAddr for use with SetPluginCellLocal().
 */
native LocalAddr GetCellLocalAddress(const any &cell);

/**
 * Gets the local-address offset of a string buffer.
 * 
 * @note            Should only be used on global or local-static variables;
 *                  stack-local addresses are invalid once the frame exits.
 * @param buffer    Buffer to query (by-ref).
 * @return          LocalAddr for use with SetPluginStringLocal().
 */
native LocalAddr GetStringLocalAddress(const char[] buffer);

/**
 * Writes a value to a plugin cell variable by local address.
 *
 * @param addr      LocalAddr from GetCellLocalAddress().
 * @param value     Value to write.
 * @error           Invalid local address.
 */
native void SetPluginCellLocal(LocalAddr addr, any value);

/**
 * Copies a string to a plugin buffer by local address.
 *
 * @param addr      LocalAddr from GetStringLocalAddress().
 * @param maxsize   Destination buffer size in bytes.
 * @param src       Source string to copy.
 * @error           Invalid local address or invalid source string.
 */
native void SetPluginStringLocal(LocalAddr addr, int maxsize, const char[] src);




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

#define _CONNECT_SIG_NOCHECK(%1,%2,%3) public const SignalSlotInfo __CSIG_%1_%2 = {false,%3,#%1,#%2};

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
public const int __include_signal__ = 0;