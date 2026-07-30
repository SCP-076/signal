

#if defined _auto_cvar
 #endinput
#endif
#define _auto_cvar

#include <signal>

/**
 * ==================================================================================================
 *  AutoCvar
 *
 *  A declarative wrapper around SourceMod ConVars that binds a cvar to a
 *  global variable outside of function bodies. The global variable stays
 *  automatically in sync with the latest cvar value — no manual updates needed.
 * ==================================================================================================
 */


/**
 * ==================================================================================================
 *  Optional config macros (define before including this file)
 *
 *  1.AUTOCVAR_DEF_FLAGS  Default FCVAR flags for CreateConVar (default: 0, no flags)
 *  2.MAX_STRING_CVAR_LEN Max buffer size for string-type AutoCvar globals (default: 2048)
 *  3.AUTOCVAR_SLOTS       Max number of AutoCvar slots per plugin (default: 32)
 *  4.AUTOCVAR_PREFIX      Prefix prepended to all cvar names created via AutoCvar
 *
 *  Example:
 *  #define AUTOCVAR_DEF_FLAGS    FCVAR_PROTECTED  // default protection flag for new cvars
 *  #define MAX_STRING_CVAR_LEN   512               // reduce string buffer cap
 *  #define AUTOCVAR_SLOTS        128               // reserve 128 slots for this plugin
 *  #define AUTOCVAR_PREFIX       "test_"           // prefix all autocvar-created cvars
 *
 *  #include <signal>
 * ==================================================================================================
 */

#if !defined AUTOCVAR_DEF_FLAGS
    #define AUTOCVAR_DEF_FLAGS 0
#endif
#if !defined MAX_STRING_CVAR_LEN
    #define MAX_STRING_CVAR_LEN 2048
#endif
#if !defined AUTOCVAR_SLOTS
    #define AUTOCVAR_SLOTS 32
#endif
#if !defined AUTOCVAR_PREFIX
    #define AUTOCVAR_PREFIX ""
#endif

/**
 * Declares an int AutoCvar.
 *
 * @param %1  Global variable name.
 * @param %2  Method chain: CreateConVar() or BindConVar(), optionally
 *            followed by .Min(a) / .Max(b) / .Bounds(a,b) / .Callback(cb).
 *
 * Usage:
 *   CVAR_INT(g_iPort, BindConVar("hostport"))
 *   CVAR_INT(g_iVal,  CreateConVar("my_int", "1", "description"))
 *   CVAR_INT(g_iVal,  CreateConVar("my_int", "50").Min(0).Max(100))
 */
#define CVAR_INT(%1,%2) \
	public const int %1;\
	_CONNECT_SIG_NOCHECK(_%1,OnAllSignalsLoaded,0)\
	public void _%1(){\
		AutoCvarFactory_Int.Init(%1).%2;\
	}

/**
 * Declares a float AutoCvar.
 *
 * @param %1  Global variable name.
 * @param %2  Method chain: CreateConVar() or BindConVar(), optionally
 *            followed by .Min(a) / .Max(b) / .Bounds(a,b) / .Callback(cb).
 *
 * Usage:
 *   CVAR_FLOAT(g_fVal, BindConVar("sv_gravity"))
 *   CVAR_FLOAT(g_fVal, CreateConVar("my_float", "1.0").Bounds(0.1, 10.0))
 */
#define CVAR_FLOAT(%1,%2) \
	public const float %1;\
	_CONNECT_SIG_NOCHECK(_%1,OnAllSignalsLoaded,0)\
	public void _%1(){\
		AutoCvarFactory_Float.Init(%1).%2;\
	}

/**
 * Declares a bool AutoCvar.
 *
 * @param %1  Global variable name.
 * @param %2  Method chain: CreateConVar() or BindConVar(), optionally
 *            followed by .Callback(cb). (Bounds are enforced [0,1] internally.)
 *
 * Usage:
 *   CVAR_BOOL(g_bVal, BindConVar("sv_cheats").Callback(OnCheatsChanged))
 *   CVAR_BOOL(g_bVal, CreateConVar("my_bool", "1"))
 */
#define CVAR_BOOL(%1,%2) \
	public const bool %1;\
	_CONNECT_SIG_NOCHECK(_%1,OnAllSignalsLoaded,0)\
	public void _%1(){\
		AutoCvarFactory_Bool.Init(%1).%2;\
	}

/**
 * Declares a string AutoCvar.
 *
 * @param %1  Global variable name.
 * @param %2  String buffer size (char[size]).
 * @param %3  Method chain: CreateConVar() or BindConVar(), optionally
 *            followed by .Callback(cb). (No bounds support for strings.)
 *
 * Usage:
 *   CVAR_STRING(g_sVal, 64, BindConVar("hostname"))
 *   CVAR_STRING(g_sVal, 32, CreateConVar("my_str", "Hello"))
 */
#define CVAR_STRING(%1,%2,%3) \
	public const char %1[%2];\
	_CONNECT_SIG_NOCHECK(_%1,OnAllSignalsLoaded,0)\
	public void _%1(){\
		AutoCvarFactory_Str.Init(%1,%2).%3;\
	}


///////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////


typeset AutoCvarCB
{
	function void(); 
	function void(AutoCvar cvar); 
	function void(AutoCvar cvar, const char[] oldVal); 
	function void(AutoCvar cvar, const char[] oldVal, const char[] newVal); 
}

static int _cvar_count;
static StringMap _infoCache;
static StringMap _pubvarCache;

static CvarInfo _cvar[AUTOCVAR_SLOTS];


///////////////////////////////////////////////////////////////////
//                  AutoCvar
///////////////////////////////////////////////////////////////////

#define this_convar _cvar[view_as<int>(this)].handle
methodmap AutoCvar
{
	// Returns the underlying ConVar handle.
	property ConVar Handle {
		public get(){return this_convar;}
	}

	// Retrieves or sets a boolean value for the convar.
	property bool BoolValue 
	{
		public get(){return this_convar.BoolValue;}
		public set(bool b){this_convar.BoolValue = b;}
	}

	// Retrieves or sets an integer value for the convar.
	property int IntValue {
		public get(){return this_convar.IntValue;}
		public set(int value){this_convar.IntValue = value;}
	}

	// Retrieves or sets a float value for the convar.
	property float FloatValue {
		public get(){return this_convar.FloatValue;}
		public set(float value){this_convar.FloatValue = value;}
	}

	// Sets the boolean value of a console variable.
	//
	// Note: The replicate and notify params are only relevant for the
	// original, Dark Messiah, and Episode 1 engines. Newer engines
	// automatically do these things when the convar value is changed.
	//
	// @param value     New boolean value.
	// @param replicate If set to true, the new convar value will be set on all clients.
	//                  This will only work if the convar has the FCVAR_REPLICATED flag
	//                  and actually exists on clients.
	// @param notify    If set to true, clients will be notified that the convar has changed.
	//                  This will only work if the convar has the FCVAR_NOTIFY flag.
	public void SetBool(bool value, bool replicate=false, bool notify=false)
	{
		this_convar.SetBool(value,replicate,notify);
	}

	// Sets the integer value of a console variable.
	//
	// Note: The replicate and notify params are only relevant for the
	// original, Dark Messiah, and Episode 1 engines. Newer engines
	// automatically do these things when the convar value is changed.
	//
	// @param value     New integer value.
	// @param replicate If set to true, the new convar value will be set on all clients.
	//                  This will only work if the convar has the FCVAR_REPLICATED flag
	//                  and actually exists on clients.
	// @param notify    If set to true, clients will be notified that the convar has changed.
	//                  This will only work if the convar has the FCVAR_NOTIFY flag.
	public void SetInt(int value, bool replicate=false, bool notify=false)
	{
		this_convar.SetInt(value,replicate,notify);
	}

	// Sets the floating point value of a console variable.
	//
	// Note: The replicate and notify params are only relevant for the
	// original, Dark Messiah, and Episode 1 engines. Newer engines
	// automatically do these things when the convar value is changed.
	//
	// @param value     New floating point value.
	// @param replicate If set to true, the new convar value will be set on all clients.
	//                  This will only work if the convar has the FCVAR_REPLICATED flag
	//                  and actually exists on clients.
	// @param notify    If set to true, clients will be notified that the convar has changed.
	//                  This will only work if the convar has the FCVAR_NOTIFY flag.
	public void SetFloat(float value, bool replicate=false, bool notify=false)
	{
		this_convar.SetFloat(value,replicate,notify);
	}

	// Retrieves the string value of a console variable.
	//
	// @param value      Buffer to store the value of the convar.
	// @param maxlength  Maximum length of string buffer.
	public void GetString(char[] value, int maxlength)
	{
		this_convar.GetString(value,maxlength);
	}

	// Sets the string value of a console variable.
	//
	// Note: The replicate and notify params are only relevant for the
	// original, Dark Messiah, and Episode 1 engines. Newer engines
	// automatically do these things when the convar value is changed.
	//
	// @param value      New string value.
	// @param replicate  If set to true, the new convar value will be set on all clients.
	//                   This will only work if the convar has the FCVAR_REPLICATED flag
	//                   and actually exists on clients.
	// @param notify     If set to true, clients will be notified that the convar has changed.
	//                   This will only work if the convar has the FCVAR_NOTIFY flag.
	public void SetString(const char[] value, bool replicate=false, bool notify=false)
	{
		this_convar.SetString(value,replicate,notify);
	}

	// Sets or removes (pass null) the change callback.
	public AutoCvar SetCallback(AutoCvarCB func)
	{
		return _SetCallback(this, func);
	}

}
#undef this_convar



///////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////
// Retrieve an AutoCvar by global variable reference.
stock AutoCvar GetAutoCvar(const any &pubvar)
{
	AutoCvar index;
	if(!_pubvarCache)
		ThrowError("No AutoCvar registered in this plugin");
	if(!_pubvarCache.GetValue(_MapKey(GetCellLocalAddress(pubvar)),index))
		ThrowError("AutoCvar not found for the given variable");
	return index;
}

stock AutoCvar GetAutoStringCvar(const char[] pubvar)
{
	AutoCvar index;
	if(!_pubvarCache)
		ThrowError("No AutoCvar registered in this plugin");
	if(!_pubvarCache.GetValue(_MapKey(GetStringLocalAddress(pubvar)),index))
		ThrowError("AutoCvar not found for the given variable");
	return index;
}

// Returns the total number of registered AutoCvars.
stock int GetAutoCvarCount()
{
	return _cvar_count;
}



///////////////////////////////////////////////////////////////////
//                       Internal helpers
///////////////////////////////////////////////////////////////////



enum CvarType
{
	CvarType_Bool = 0,
	CvarType_Int,
	CvarType_Float,
	CvarType_String,
}

enum struct CvarInfo
{
	CvarType type;
	ConVar handle;
	LocalAddr local;  // local address of the bound global variable
	int size;         // buffer size in bytes (only used by string cvars)
	AutoCvarCB cb;
	
	void Reset()
	{
		static CvarInfo def;
		this = def;
	}
	void UpdatePubVar()
	{
		switch(this.type)
		{
			case CvarType_Bool:{SetPluginCellLocal(this.local,this.handle.BoolValue);}
			case CvarType_Int:{SetPluginCellLocal(this.local,this.handle.IntValue);}
			case CvarType_Float:{SetPluginCellLocal(this.local,this.handle.FloatValue);}
			case CvarType_String:
			{
				static char buffer[MAX_STRING_CVAR_LEN];
				this.handle.GetString(buffer, this.size);
				SetPluginStringLocal(this.local,this.size,buffer);
			}
		}
	}

	void CallUpdated(int index,const char[] oldValue, const char[] newValue)
	{
		if (this.cb)
		{
			Call_StartFunction(INVALID_HANDLE, this.cb);
			Call_PushCell(index);
			Call_PushString(oldValue);
			Call_PushString(newValue);
			Call_Finish();
		}
	}

}

methodmap AutoCvarBinder
{
	public AutoCvarBinder Callback(AutoCvarCB func)
	{
		return _SetCallback(this, func);
	}

	public AutoCvar ToAutoCvar()
	{
		return view_as<AutoCvar>(this);
	}
}



methodmap AutoCvarFactory_Str
{
	public static AutoCvarFactory_Str Init(const char[] pubvar, int size)
	{
		return _InitAutoCvar(CvarType_String, .buffer = pubvar, .stringsize = size);
	}
	public AutoCvarBinder BindConVar(const char[] name)
	{
		return _FinishBind(this, name);
	}
	public AutoCvarBinder CreateConVar(
		const char[] name,
		const char[] defaultValue,
		const char[] description="",
		int flags = AUTOCVAR_DEF_FLAGS)
	{
		return _CreateAutoCvar(this, name,defaultValue,description,flags);
	}
}

///////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////
methodmap AutoCvarFactory_Int
{
	public static AutoCvarFactory_Int Init(const int &pubvar)
	{
		return _InitAutoCvar(CvarType_Int,pubvar);
	}
	public AutoCvarBinder BindConVar(const char[] name)
	{
		return _FinishBind(this, name);
	}
	public AutoIntCvarCreator CreateConVar(
		const char[] name,
		const char[] defaultValue,
		const char[] description="",
		int flags = AUTOCVAR_DEF_FLAGS)
	{
		return _CreateAutoCvar(this, name,defaultValue,description,flags);
	}
}

// Extended creator with Min/Max/Bounds support for int cvars.
methodmap AutoIntCvarCreator < AutoCvarBinder
{
	public AutoIntCvarCreator Callback(AutoCvarCB func)
	{
		return _SetCallback(this, func);
	}

	public AutoIntCvarCreator Min(int min)
	{
		_SetBounds(this,ConVarBound_Lower,true,float(min));
		return this;
	}
	public AutoIntCvarCreator Max(int max)
	{
		_SetBounds(this,ConVarBound_Upper,true,float(max));
		return this;
	}
	public AutoIntCvarCreator Bounds(int min,int max)
	{
		_SetBounds(this,ConVarBound_Lower,true,float(min));
		_SetBounds(this,ConVarBound_Upper,true,float(max));
		return this;
	}
}

///////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////

methodmap AutoCvarFactory_Bool
{
	public static AutoCvarFactory_Bool Init(const bool &pubvar)
	{
		return _InitAutoCvar(CvarType_Bool, pubvar);
	}
	public AutoCvarBinder BindConVar(const char[] name)
	{
		return _FinishBind(this, name);
	}
	public AutoCvarBinder CreateConVar(
		const char[] name,
		const char[] defaultValue,
		const char[] description="",
		int flags = AUTOCVAR_DEF_FLAGS)
	{
		return _CreateAutoCvar(this, name,defaultValue,description,flags,true,0.0,true,1.0); // enforce [0,1] bounds for bool
	}
}

///////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////
methodmap AutoFloatCvarCreator < AutoCvarBinder
{
	public AutoFloatCvarCreator Callback(AutoCvarCB func)
	{
		return _SetCallback(this, func);
	}
	public AutoFloatCvarCreator Min(float min)
	{
		_SetBounds(this,ConVarBound_Lower,true,min);
		return this;
	}
	public AutoFloatCvarCreator Max(float max)
	{
		_SetBounds(this,ConVarBound_Upper,true,max);
		return this;
	}
	public AutoFloatCvarCreator Bounds(float min,float max)
	{
		_SetBounds(this,ConVarBound_Lower,true,min);
		_SetBounds(this,ConVarBound_Upper,true,max);
		return this;
	}
}

methodmap AutoCvarFactory_Float
{
	public static AutoCvarFactory_Float Init(const float &pubvar)
	{
		return _InitAutoCvar(CvarType_Float, pubvar);
	}
	public AutoCvarBinder BindConVar(const char[] name)
	{
		return _FinishBind(this, name);
	}
	public AutoFloatCvarCreator CreateConVar(
		const char[] name,
		const char[] defaultValue,
		const char[] description="",
		int flags = AUTOCVAR_DEF_FLAGS)
	{
		return _CreateAutoCvar(this,name,defaultValue,description,flags);
	}
}




static any _SetCallback(any index, AutoCvarCB func)
{
	if(!_cvar[index].handle)
		ThrowError("AutoCvar not initialized (index %i)",index);
	_cvar[index].cb = func;
	return index;
}

static any _SetBounds(any index, ConVarBounds type, bool set, float value=0.0)
{
	if(!_cvar[index].handle)
		ThrowError("AutoCvar not initialized (index %i)",index);
	_cvar[index].handle.SetBounds(type,set,value);
	return index;
}


static any _InitAutoCvar(CvarType type,const any &pubvar = 0,const char[] buffer = "",int stringsize= 0)
{
	int index = _cvar_count;
	if(index >= sizeof(_cvar))
		ThrowError("AutoCvar slot overflow (%i/%i). Increase AUTOCVAR_SLOTS.", index,sizeof(_cvar));
	if(type == CvarType_String )
	{
		if(stringsize < 2)
			ThrowError("Invalid string buffer size: %i (minimum 2)",stringsize);
		if(stringsize > MAX_STRING_CVAR_LEN)
			ThrowError("String buffer too large: %i (max %i)",stringsize, MAX_STRING_CVAR_LEN);
	}
	LocalAddr local = (type == CvarType_String)? GetStringLocalAddress(buffer):GetCellLocalAddress(pubvar);
	_cvar[index].Reset();
	_cvar[index].type = type;
	_cvar[index].local = local;
	_cvar[index].size = stringsize;
	return index;
}


static any _CreateAutoCvar(any index,
	const char[] name,
	const char[] defaultValue,
	const char[] description="",
	int flags=0,
	bool hasMin=false, float min=0.0,
	bool hasMax=false, float max=0.0)
{
	char sName[128];
	FormatEx(sName,sizeof(sName),"%s%s",AUTOCVAR_PREFIX,name);
	CreateConVar(sName,defaultValue,description,flags,hasMin,min,hasMax,max);
	return _FinishBind(index, sName);
}

static any _FinishBind(any index,const char[] name)
{
	if(!_cvar[index].local)
		ThrowError("AutoCvar not initialized");
	if(_cvar[index].handle)
		ThrowError("AutoCvar already bound");

	ConVar handle = FindConVar(name);
	if (!handle)
		ThrowError("ConVar not found: %s", name);

	if (!_infoCache)
	{
		_infoCache = new StringMap();
		_pubvarCache = new StringMap();
	}
	if (_infoCache.ContainsKey(_MapKey(handle)))
		ThrowError("ConVar \"%s\" already bound by another AutoCvar", name);
	// Store handle and fetch initial value
	_cvar[index].handle = handle;
	_cvar[index].UpdatePubVar();
	handle.AddChangeHook(_OnAutoCvarChanged);
	// Cache mappings
	_infoCache.SetValue(_MapKey(handle),index);
	_pubvarCache.SetValue(_MapKey(_cvar[index].local),index);
	_cvar_count++;
	return index;
}


static void _OnAutoCvarChanged(ConVar handle, const char[] oldValue, const char[] newValue)
{
	int index;
	if (!_infoCache.GetValue(_MapKey(handle), index))
		ThrowError("Cannot find cvar handle %i", handle);
	_cvar[index].UpdatePubVar();
	_cvar[index].CallUpdated(index,oldValue,newValue);
}


static char[] _MapKey(any value)
{
	static char key[6];
	key[0] = view_as<char>(((value >>> 28) & 0x7F) | 0x80);
	key[1] = view_as<char>(((value >>> 21) & 0x7F) | 0x80);
	key[2] = view_as<char>(((value >>> 14) & 0x7F) | 0x80);
	key[3] = view_as<char>(((value >>> 7) & 0x7F) | 0x80);
	key[4] = view_as<char>((value & 0x7F) | 0x80);
	key[5] = '\0';
	return key;
}