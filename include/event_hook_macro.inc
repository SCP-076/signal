#if defined _event_hook_macro
	#endinput
#endif
#define _event_hook_macro

#include <signal>


/**
 * ==================================================================================================
 * Provides macro wrappers for hooking SourceMod events.
 *
 * Allows for the declarative creation of event hooks and their callbacks anywhere in your scripts.
 * (Achieved by automatically wiring the registration to the OnAllSignalsLoaded signal).
 * ==================================================================================================
 */

/**
 * Hooks a game event before it is fired (Pre-hook) and allows altering or blocking it.
 * Expects the callback to return an Action (e.g., Plugin_Continue, Plugin_Handled).
 *
 * @param %1        Event name (e.g., player_say).
 */
#define ON_EVENT_PRE_ACTION(%1) CONNECT_SIG(%1_HooKEventPre,OnAllSignalsLoaded) public void %1_HooKEventPre(){_HookEventTrim(#%1,%1_EventPreCallback,EventHookMode_Pre);} Action %1_EventPreCallback(Event event, const char[] name, bool dontBroadcast)

/**
 * Hooks a game event before it is fired (Pre-hook) for read-only purposes.
 * Expects the callback to return void.
 *
 * @param %1        Event name (e.g., player_say).
 */
#define ON_EVENT_PRE(%1) CONNECT_SIG(%1_HooKEventPre,OnAllSignalsLoaded) public void %1_HooKEventPre(){_HookEventTrim(#%1,%1_EventPreCallback,EventHookMode_Pre);} void %1_EventPreCallback(Event event, const char[] name, bool dontBroadcast)

/**
 * Hooks a game event after it is fired (Post-hook).
 * The event data is copied, allowing for delayed processing if needed. Expects the callback to return void.
 *
 * @param %1        Event name (e.g., player_say).
 */
#define ON_EVENT_POST(%1) CONNECT_SIG(%1_HooKEventPost,OnAllSignalsLoaded) public void %1_HooKEventPost(){_HookEventTrim(#%1,%1_EventPostCallback,EventHookMode_Post);} void %1_EventPostCallback(Event event, const char[] name, bool dontBroadcast)

/**
 * Hooks a game event after it is fired (Post-hook) without copying the event data.
 * More efficient, but the event handle cannot be safely passed into timers. Expects the callback to return void.
 *
 * @param %1        Event name (e.g., player_say).
 */
#define ON_EVENT_POST_NO_COPY(%1) CONNECT_SIG(%1_HooKEventPost,OnAllSignalsLoaded) public void %1_HooKEventPost(){_HookEventTrim(#%1,%1_EventPostCallback,EventHookMode_PostNoCopy);} void %1_EventPostCallback(Event event, const char[] name, bool dontBroadcast)



//Internal helper to eliminate potential trailing/leading whitespace 
stock void _HookEventTrim(const char[] name, EventHook callback,EventHookMode mode)
{
	char buffer[128];
    strcopy(buffer, sizeof(buffer), name);
    TrimString(buffer);
	HookEvent(buffer,callback,mode);
}


/*
 ============================================================================
 Usage Examples
 ============================================================================

	ON_EVENT_PRE_ACTION(player_say)
	{
		char sbuff[256];
		int client = GetClientOfUserId(event.GetInt("userid"));
		event.GetString("text",sbuff,sizeof(sbuff));

		PrintToServer("ON_EVENT_PRE_ACTION: player_say %N %s",client,sbuff);
		return Plugin_Continue;
	}


	ON_EVENT_PRE(player_say)
	{
		char sbuff[256];
		int client = GetClientOfUserId(event.GetInt("userid"));
		event.GetString("text",sbuff,sizeof(sbuff));

		PrintToServer("ON_EVENT_PRE: player_say %N %s",client,sbuff);
	}



	ON_EVENT_POST(player_say)
	{
		char sbuff[256];
		int client = GetClientOfUserId(event.GetInt("userid"));
		event.GetString("text",sbuff,sizeof(sbuff));
		
		PrintToServer("ON_EVENT_POST: player_say %N %s",client,sbuff);
		
	}


	ON_EVENT_POST_NO_COPY(player_say)
	{
		PrintToServer("ON_EVENT_POST_NO_COPY: player_say");
	}





*/