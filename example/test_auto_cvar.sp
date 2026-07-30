#pragma semicolon 1
#pragma newdecls required

#include <sourcemod>
#include <auto_cvar>



////////////////////////////////////////////////////////////////////////////////////////////////
// 1: BindConVar — bind an existing engine cvar to a typed global variable
////////////////////////////////////////////////////////////////////////////////////////////////

// BindConVar(const char[] name)
CVAR_INT    (g_iHostport,      BindConVar("hostport"))// equivalent to: const int  g_iHostport;
CVAR_STRING (g_sHostname, 64,  BindConVar("hostname"))// equivalent to: const char g_sHostname[64]

void TestPrint1()
{
	// bound globals auto-sync — read directly, no manual update needed
	PrintToServer("hostport is %i", g_iHostport);
	PrintToServer("hostname is %s", g_sHostname);
	
}


////////////////////////////////////////////////////////////////////////////////////////////////
// 2: Chain a callback when the cvar value changes
////////////////////////////////////////////////////////////////////////////////////////////////

// Callback(AutoCvarCB func)
CVAR_BOOL  (g_bCheats, BindConVar("sv_cheats").Callback(OnCheatUpdated))

void OnCheatUpdated(AutoCvar cvar, const char[] oldVal, const char[] newVal)
{
	if (g_bCheats)
		PrintToServer("cheat is enabled");
	else
		PrintToServer("cheat is disabled");
}

////////////////////////////////////////////////////////////////////////////////////////////////
// 3: CreateConVar — create a plugin-owned cvar
////////////////////////////////////////////////////////////////////////////////////////////////
// Signature mirrors the SM native CreateConVar:
// CreateConVar(
// 	 const char[] name,
// 	 const char[] defaultValue,
// 	 const char[] description = "",
// 	 int flags = 0)
CVAR_INT   (g_myCvar,    CreateConVar("my_cvar", "123", "example cvar", FCVAR_NOTIFY).Callback(OnMyCvarChanged))



// Optional bounds chaining (int & float only):
//   .Min(a)      — lower bound
//   .Max(b)      — upper bound
//   .Bounds(a,b) — both bounds
CVAR_INT   (g_iCvarMin,   CreateConVar("cvar_int_min",  "0"    ).Min(0))            // int Min
CVAR_INT   (g_iCvarMax,   CreateConVar("cvar_int_max",  "100"  ).Max(100))          // int Max
CVAR_INT   (g_iCvarRange, CreateConVar("cvar_int_range","50"   ).Bounds(0, 100))    // int Bounds

CVAR_FLOAT (g_fCvarMin,   CreateConVar("cvar_flt_min",  "0.0"  ).Min(0.0))          // float Min
CVAR_FLOAT (g_fCvarMax,   CreateConVar("cvar_flt_max",  "1.0"  ).Max(1.0))          // float Max
CVAR_FLOAT (g_fCvarRange, CreateConVar("cvar_flt_range","0.5"  ).Bounds(0.0, 1.0))  // float Bounds

CVAR_STRING(g_sCvar,  32, CreateConVar("cvar_str",      "ABC"))                     // string: no bounds
CVAR_BOOL  (g_bCvar,      CreateConVar("cvar_bool",     "1"))                       // bool: [0,1] enforced

void TestPrint2()
{
	PrintToServer("my_cvar:       %i", g_myCvar);

	PrintToServer("cvar_int_min:  %i", g_iCvarMin);
	PrintToServer("cvar_int_max:  %i", g_iCvarMax);
	PrintToServer("cvar_int_range:%i", g_iCvarRange);
	PrintToServer("cvar_flt_min:  %f", g_fCvarMin);
	PrintToServer("cvar_flt_max:  %f", g_fCvarMax);
	PrintToServer("cvar_flt_range:%f", g_fCvarRange);
	PrintToServer("cvar_bool:     %i", g_bCvar);
	PrintToServer("cvar_str:      %s", g_sCvar);

	// Retrieve the AutoCvar object to set values by type
	GetAutoCvar(g_iCvarMin).IntValue   += 1;
	GetAutoCvar(g_fCvarMin).FloatValue += 1.0;
	GetAutoCvar(g_bCvar).BoolValue    = false;
	GetAutoStringCvar(g_sCvar).SetString("aaa");
	

}

void OnMyCvarChanged(AutoCvar cvar, const char[] oldVal, const char[] newVal)
{
	PrintToServer("any convar changed %s to %s", oldVal, newVal);
}

public void OnPluginStart()
{
	TestPrint1();
	TestPrint2();
}
