#include "utils.h"
#include "../../../add_on/weakref/weakref.h"

namespace Test_Addon_WeakRef
{

static const char *TESTNAME = "Test_Addon_WeakRef";

class MyClass
{
public:
	MyClass() { refCount = 1; weakRefFlag = 0; }
	~MyClass()
	{
		if( weakRefFlag )
			weakRefFlag->Release();
	}
	void AddRef() { refCount++; }
	void Release() 
	{
		// If the weak ref flag exists it is because someone held a weak ref
		// and that someone may add a reference to the object at any time. It
		// is ok to check the existance of the weakRefFlag without locking here
		// because if the refCount is 1 then no other thread is currently 
		// creating the weakRefFlag.
		if( refCount == 1 && weakRefFlag )
		{
			// Set the flag to tell others that the object is no longer alive
			// We must do this before decreasing the refCount to 0 so we don't
			// end up with a race condition between this thread attempting to 
			// destroy the object and the other that temporary added a strong
			// ref from the weak ref.
			weakRefFlag->Set(true);
		}

		if( --refCount == 0 ) 
			delete this; 
	}
	asILockableSharedBool *GetWeakRefFlag()
	{
		if( !weakRefFlag )
		{
			// Lock globally so no other thread can attempt
			// to create a shared bool at the same time
			asAcquireExclusiveLock();

			// Make sure another thread didn't create the 
			// flag while we waited for the lock
			if( !weakRefFlag )
				weakRefFlag = asCreateLockableSharedBool();

			asReleaseExclusiveLock();
		}

		return weakRefFlag;
	}

	static MyClass *Factory() { return new MyClass(); }

protected:
	int refCount;
	asILockableSharedBool *weakRefFlag;
};

bool Test()
{
	bool fail = false;
	int r;
	COutStream out;
	CBufferedOutStream bout;
	asIScriptEngine *engine;

	// Ordinary tests
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(COutStream,Callback), &out, asCALL_THISCALL);
		RegisterScriptWeakRef(engine);
		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);

		const char *script =
			"class Test {} \n"
			"void main() { \n"
			"  Test @t = Test(); \n"
			"  weakref<Test> r(t); \n"
			"  assert( r.get() !is null ); \n"
			"  @t = null; \n"
			"  assert( r.get() is null ); \n"
			"} \n";

		asIScriptModule *mod = engine->GetModule("test", asGM_ALWAYS_CREATE);
		mod->AddScriptSection(TESTNAME, script);
		r = mod->Build();
		if( r < 0 )
		{
			TEST_FAILED;
			PRINTF("%s: Failed to compile the script\n", TESTNAME);
		}

		r = ExecuteString(engine, "main()", mod);
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;

		engine->Release();
	}

	// It shouldn't be possible to instanciate the weakref for types that do not support it
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream,Callback), &bout, asCALL_THISCALL);
		bout.buffer = "";
		RegisterScriptWeakRef(engine);
		RegisterStdString(engine);
		RegisterScriptArray(engine, false);
		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);

		const char *script =
			"class Test {} \n"
			"void main() { \n"
			"  weakref<int> a; \n"         // fail
			"  weakref<string> b; \n"      // fail
			"  weakref<Test@> c; \n"       // fail
			"  weakref<array<Test>> d; \n" // fail
			"} \n";

		asIScriptModule *mod = engine->GetModule("test", asGM_ALWAYS_CREATE);
		mod->AddScriptSection(TESTNAME, script);
		r = mod->Build();
		if( r >= 0 )
			TEST_FAILED;

		if( bout.buffer != "Test_Addon_WeakRef (2, 1) : Info    : Compiling void main()\n"
						   "Test_Addon_WeakRef (3, 11) : Error   : Can't instanciate template 'weakref' with subtype 'int'\n"
						   "Test_Addon_WeakRef (4, 11) : Error   : Can't instanciate template 'weakref' with subtype 'string'\n"
						   "Test_Addon_WeakRef (5, 11) : Error   : Can't instanciate template 'weakref' with subtype 'Test@'\n"
						   "weakref (0, 0) : Error   : The subtype doesn't support weak references\n"
						   "Test_Addon_WeakRef (6, 11) : Error   : Can't instanciate template 'weakref' with subtype 'array<Test>'\n" )
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		engine->Release();
	}

	// Test registering app type with weak ref
#ifndef AS_MAX_PORTABILITY
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(COutStream,Callback), &out, asCALL_THISCALL);
		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);

		engine->RegisterObjectType("MyClass", 0, asOBJ_REF);
		engine->RegisterObjectBehaviour("MyClass", asBEHAVE_FACTORY, "MyClass @f()", asFUNCTION(MyClass::Factory), asCALL_CDECL);
		engine->RegisterObjectBehaviour("MyClass", asBEHAVE_ADDREF, "void f()", asMETHOD(MyClass, AddRef), asCALL_THISCALL);
		engine->RegisterObjectBehaviour("MyClass", asBEHAVE_RELEASE, "void f()", asMETHOD(MyClass, Release), asCALL_THISCALL);
		engine->RegisterObjectBehaviour("MyClass", asBEHAVE_GET_WEAKREF_FLAG, "int &f()", asMETHOD(MyClass, GetWeakRefFlag), asCALL_THISCALL);

		RegisterScriptWeakRef(engine);

		const char *script =
			"void main() { \n"
			"  MyClass @t = MyClass(); \n"
			"  weakref<MyClass> r(t); \n"
			"  assert( r.get() !is null ); \n"
			"  @t = null; \n"
			"  assert( r.get() is null ); \n"
			"} \n";

		asIScriptModule *mod = engine->GetModule("test", asGM_ALWAYS_CREATE);
		mod->AddScriptSection(TESTNAME, script);
		r = mod->Build();
		if( r < 0 )
		{
			TEST_FAILED;
			PRINTF("%s: Failed to compile the script\n", TESTNAME);
		}

		r = ExecuteString(engine, "main()", mod);
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;

		engine->Release();
	}
#endif

	// TODO: weak: It should be possible to declare a script class to not allow weak references, and as such save the memory for the internal pointer
	// TODO: weak: add engine property to turn off automatic support for weak references to all script classes

	// Success
	return fail;
}


} // namespace

