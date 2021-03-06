#include "utils.h"
using namespace std;

static const char * const TESTNAME = "TestConstructor";

static const char *script1 =
"obj g_obj1 = g_obj2;                      \n"
"obj g_obj2();                             \n"
"obj g_obj3(12, 3);                        \n";

static const char *script2 = 
"void TestConstructor()                    \n"
"{                                         \n"
"  obj l_obj1;                             \n"
"  l_obj1.a = 5; l_obj1.b = 7;             \n"
"  obj l_obj2();                           \n"
"  obj l_obj3(3, 4);                       \n"
"  a = l_obj1.a + l_obj2.a + l_obj3.a;     \n"
"  b = l_obj1.b + l_obj2.b + l_obj3.b;     \n"
"}                                         \n";
/*
// Illegal situations
static const char *script3 = 
"obj *g_obj4();                            \n";
*/
// Using constructor to create temporary object
static const char *script4 = 
"void TestConstructor2()                   \n"
"{                                         \n"
"  a = obj(11, 2).a;                       \n"
"  b = obj(23, 13).b;                      \n"
"}                                         \n";

class CTestConstructor
{
public:
	CTestConstructor() {a = 0; b = 0;}
	CTestConstructor(int a, int b) {this->a = a; this->b = b;}

	int a;
	int b;
};

void ConstrObj(CTestConstructor *obj)
{
	new(obj) CTestConstructor();
}

void ConstrObj(int a, int b, CTestConstructor *obj)
{
	new(obj) CTestConstructor(a,b);
}

void ConstrObj_gen1(asIScriptGeneric *gen)
{
	CTestConstructor *obj = (CTestConstructor*)gen->GetObject();
	new(obj) CTestConstructor();
}

void ConstrObj_gen2(asIScriptGeneric *gen)
{
	CTestConstructor *obj = (CTestConstructor*)gen->GetObject();
	int a = gen->GetArgDWord(0);
	int b = gen->GetArgDWord(1);
	new(obj) CTestConstructor(a,b);
}


bool TestConstructor()
{
	bool fail = false;

	asIScriptEngine *engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);

	RegisterScriptString_Generic(engine);

	int r;
	r = engine->RegisterObjectType("obj", sizeof(CTestConstructor), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_C); assert( r >= 0 );
	if( strstr(asGetLibraryOptions(), "AS_MAX_PORTABILITY") )
	{
		r = engine->RegisterObjectBehaviour("obj", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(ConstrObj_gen1), asCALL_GENERIC); assert( r >= 0 );
		r = engine->RegisterObjectBehaviour("obj", asBEHAVE_CONSTRUCT, "void f(int,int)", asFUNCTION(ConstrObj_gen2), asCALL_GENERIC); assert( r >= 0 );
	}
	else
	{
		r = engine->RegisterObjectBehaviour("obj", asBEHAVE_CONSTRUCT, "void f()", asFUNCTIONPR(ConstrObj, (CTestConstructor *), void), asCALL_CDECL_OBJLAST); assert( r >= 0 );
		r = engine->RegisterObjectBehaviour("obj", asBEHAVE_CONSTRUCT, "void f(int,int)", asFUNCTIONPR(ConstrObj, (int, int, CTestConstructor *), void), asCALL_CDECL_OBJLAST); assert( r >= 0 );
	}

	r = engine->RegisterObjectProperty("obj", "int a", offsetof(CTestConstructor, a)); assert( r >= 0 );
	r = engine->RegisterObjectProperty("obj", "int b", offsetof(CTestConstructor, b)); assert( r >= 0 );

	int a, b;
	r = engine->RegisterGlobalProperty("int a", &a); assert( r >= 0 );
	r = engine->RegisterGlobalProperty("int b", &b); assert( r >= 0 );

	CBufferedOutStream out;	
	asIScriptModule *mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
	mod->AddScriptSection(TESTNAME, script1, strlen(script1), 0);
	engine->SetMessageCallback(asMETHOD(CBufferedOutStream,Callback), &out, asCALL_THISCALL);
	mod->Build();

	if( out.buffer != "" )
	{
		fail = true;
		printf("%s: Failed to compile global constructors\n", TESTNAME);
	}

	mod->AddScriptSection(TESTNAME, script2, strlen(script2));
	mod->Build();

	if( out.buffer != "" )
	{
		fail = true;
		printf("%s: Failed to compile local constructors\n", TESTNAME);
	}


	ExecuteString(engine, "TestConstructor()", mod);

	if( a != 8 || b != 11 )
	{
		printf("%s: Values are not what were expected\n", TESTNAME);
		fail = false;
	}

/*
	mod->AddScriptSection(0, TESTNAME, script3, strlen(script3));
	mod->Build(0);

	if( out.buffer != "TestConstructor (1, 12) : Info    : Compiling obj* g_obj4\n"
	                  "TestConstructor (1, 12) : Error   : Only objects have constructors\n" )
	{
		fail = true;
		printf("%s: Failed to compile global constructors\n", TESTNAME);
	}
*/
	out.buffer = "";
	mod->AddScriptSection(TESTNAME, script4, strlen(script4));
	mod->Build();

	if( out.buffer != "" ) 
	{
		fail = true;
		printf(out.buffer.c_str());
		printf("%s: Failed to compile constructor in expression\n", TESTNAME);
	}

	ExecuteString(engine, "TestConstructor2()", mod);

	if( a != 11 || b != 13 )
	{
		printf("%s: Values are not what were expected\n", TESTNAME);
		fail = false;
	}

	engine->Release();

	// Success
	return fail;
}
