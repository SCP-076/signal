#if defined _console_macro
	#endinput
#endif
#define _console_macro

#include <signal>

/**
 * ==================================================================================================
 * Provides macro wrappers for the SourceMod standard "console" library.
 *
 * Allows for the declarative creation of commands and their callbacks anywhere in your scripts.
 * (Achieved by automatically wiring the registration to the OnAllSignalsLoaded signal).
 * ==================================================================================================
 */



/**
 * Creates a Server Command.
 *
 * @param %1        Command name (e.g., sm_mycmd).
 * @param %2        Command description.
 */
#define SERVER_COMMAND(%1,%2) CONNECT_SIG(%1_RegCmd,OnAllSignalsLoaded) public void %1_RegCmd(){RegServerCmd(#%1,%1_callback,%2);} Action %1_callback(int args)

/**
 * Creates a Server Command with specific flags.
 *
 * @param %1        Command name.
 * @param %2        Command description.
 * @param %3        Command flags (FCVAR_* bitmask).
 */
#define SERVER_COMMAND_F(%1,%2,%3) CONNECT_SIG(%1_RegCmd,OnAllSignalsLoaded) public void %1_RegCmd(){RegServerCmd(#%1,%1_callback,%2,%3);} Action %1_callback(int args)

/**
 * Creates a Console Command (accessible by players).
 *
 * @param %1        Command name.
 * @param %2        Command description.
 */
#define CONSOLE_COMMAND(%1,%2) CONNECT_SIG(%1_RegCmd,OnAllSignalsLoaded) public void %1_RegCmd(){RegConsoleCmd(#%1,%1_callback,%2);} Action %1_callback(int client,int args)

/**
 * Creates a Console Command with specific flags.
 *
 * @param %1        Command name.
 * @param %2        Command description.
 * @param %3        Command flags (FCVAR_* bitmask).
 */
#define CONSOLE_COMMAND_F(%1,%2,%3) CONNECT_SIG(%1_RegCmd,OnAllSignalsLoaded) public void %1_RegCmd(){RegConsoleCmd(#%1,%1_callback,%2,%3);} Action %1_callback(int client,int args)

/**
 * Creates a standard Admin Command without a command group.
 *
 * @param %1        Command name.
 * @param %2        Admin flags required (ADMFLAG_* bitmask).
 * @param %3        Command description.
 */
#define ADMIN_COMMAND(%1,%2,%3) CONNECT_SIG(%1_RegCmd,OnAllSignalsLoaded) public void %1_RegCmd(){RegAdminCmd(#%1,%1_callback,%2,%3);} Action %1_callback(int client,int args)

/**
 * Creates a standard Admin Command with specific console flags.
 *
 * @param %1        Command name.
 * @param %2        Admin flags required (ADMFLAG_* bitmask).
 * @param %3        Command description.
 * @param %4        Command flags (FCVAR_* bitmask).
 */
#define ADMIN_COMMAND_F(%1,%2,%3,%4) CONNECT_SIG(%1_RegCmd,OnAllSignalsLoaded) public void %1_RegCmd(){RegAdminCmd(#%1,%1_callback,%2,%3,"",%4);} Action %1_callback(int client,int args)

/**
 * Creates an Admin Command assigned to a specific command group.
 *
 * @param %1        Command name.
 * @param %2        Admin flags required (ADMFLAG_* bitmask).
 * @param %3        Command group name.
 * @param %4        Command description.
 */
#define ADMIN_GROUP_COMMAND(%1,%2,%3,%4) CONNECT_SIG(%1_RegCmd,OnAllSignalsLoaded) public void %1_RegCmd(){RegAdminCmd(#%1,%1_callback,%2,%4,%3);} Action %1_callback(int client,int args)

/**
 * Creates an Admin Command with a command group and console flags.
 *
 * @param %1        Command name.
 * @param %2        Admin flags required (ADMFLAG_* bitmask).
 * @param %3        Command group name.
 * @param %4        Command description.
 * @param %5        Command flags (FCVAR_* bitmask).
 */
#define ADMIN_GROUP_COMMAND_F(%1,%2,%3,%4,%5) CONNECT_SIG(%1_RegCmd,OnAllSignalsLoaded) public void %1_RegCmd(){RegAdminCmd(#%1,%1_callback,%2,%4,%3,%5);} Action %1_callback(int client,int args)


/*
 ============================================================================
 Usage Examples
 ============================================================================

	SERVER_COMMAND(sm_server1, "This is a server command")
	{
		PrintToServer("Triggered sm_server1 (args=%i)", args);
		return Plugin_Handled;
	}

	SERVER_COMMAND_F(sm_server2, "This is a server command with flags", FCVAR_NONE)
	{
		PrintToServer("Triggered sm_server2 (args=%i)", args);
		return Plugin_Handled;
	}


	CONSOLE_COMMAND(sm_console1, "This is a console command")
	{
		PrintToServer("%N used sm_console1 (args=%i)", client, args);
		return Plugin_Handled;
	}

	CONSOLE_COMMAND_F(sm_console2, "This is a console command with flags", FCVAR_NONE)
	{
		PrintToServer("%N used sm_console2 (args=%i)", client, args);
		return Plugin_Handled;
	}


	ADMIN_COMMAND(sm_admin1, ADMFLAG_ROOT, "This is an admin command")
	{
		PrintToServer("Admin %N used sm_admin1 (args=%i)", client, args);
		return Plugin_Handled;
	}

	ADMIN_COMMAND_F(sm_admin2, ADMFLAG_ROOT, "This is an admin command with flags", FCVAR_NONE)
	{
		PrintToServer("Admin %N used sm_admin2 (args=%i)", client, args);
		return Plugin_Handled;
	}

	ADMIN_GROUP_COMMAND(sm_admingroup1, ADMFLAG_ROOT, "my_group", "This is a grouped admin command")
	{
		PrintToServer("Admin %N used sm_admingroup1 (args=%i)", client, args);
		return Plugin_Handled;
	}

	ADMIN_GROUP_COMMAND_F(sm_admingroup2, ADMFLAG_ROOT, "my_group", "This is a grouped admin command with flags", FCVAR_NONE)
	{
		PrintToServer("Admin %N used sm_admingroup2 (args=%i)", client, args);
		return Plugin_Handled;
	}
*/