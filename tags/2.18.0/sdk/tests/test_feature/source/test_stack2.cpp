
#include <stdarg.h>
#include "utils.h"

using std::string;

namespace TestStack2
{

static const char * const TESTNAME = "TestStack2";

static const char *script1 =
"void testargs()                   \n"
"{                                 \n"
"  t(\"a\",\"b\");                 \n"
"  string c; int d = 0;            \n"
"  s(c, d);                        \n"
"}                                 \n"
"void t(string, string)            \n"
"{}                                \n"
"void s(string &out a, int &out b) \n"
"{ a = \"\"; b = 1; }              \n";

static const char *script2 = 
"void testop()          \n"
"{                      \n"
"  \"a\"+func(\"b\");   \n"
"  string a;            \n"
"  \"a\"+a;             \n"
"}                      \n"
"string func(string)    \n"
"{                      \n"
"  return \"b\";        \n"
"}                      \n";

static const char *script3 =
"void testassign()        \n"
"{                        \n"
"  string a;              \n"
"  a = \"b\";             \n"
"  string[] v(1);         \n"
"  v[0] = a;              \n"
"  b_intref() += a_int(); \n"
"}                        \n";

static const char *script4 =
"void testmethod()      \n"
"{                      \n"
"  int[] a(5);          \n"
"  a[4];                \n"
"}                      \n";

static const char *script5 = 
"void testoutparm()          \n"
"{                           \n"
"  string a, b;              \n"
"  complex3(complex(a));     \n"
"  complex(a) = b;           \n"
"  complex2() = b;           \n"
"  if( complex(a) == b ) {}  \n"
"  if( complex3(a) == 2 ) {} \n"
"}                           \n";

string output;
int a_int()
{
	output += "a";
	return 1;
}
int b_int()
{
	output += "b";
	return 2;
}

string a_str()
{
	output += "a";
	return "a";
}

string b_str()
{
	output += "b";
	return "b";
}

string bs;
string &b_strref()
{
	output += "b";
	return bs;
}

int bi;
int &b_intref()
{
	output += "b";
	return bi;
}

string cs;
string &complex(string &str)
{
	str = "outparm";
	return cs;
}

string &complex2()
{
	return cs;
}

int ci;
int &complex3(string &str)
{
	str = "outparm3";
	return ci;
}

class CProp
{
public:
	CProp() {rc = 1;}

	void AddRef() {rc++;}
	void Release() {rc--; if( rc == 0 ) delete this;}

	void Get(string &out) {out = "PropOut";}

	int rc;
};

CProp *GetProp(string & /*in*/)
{
	// return with the ref already counted
	return new CProp();
}

bool Test()
{
	if( strstr(asGetLibraryOptions(), "AS_MAX_PORTABILITY") )
	{
		printf("%s: Skipped due to AS_MAX_PORTABILITY\n", TESTNAME);
		return false;
	}
	bool fail = false;

 	asIScriptEngine *engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);

	RegisterStdString(engine);

	engine->RegisterGlobalFunction("int a_int()", asFUNCTION(a_int), asCALL_CDECL);
	engine->RegisterGlobalFunction("int b_int()", asFUNCTION(b_int), asCALL_CDECL);
	engine->RegisterGlobalFunction("string a_str()", asFUNCTION(a_str), asCALL_CDECL);
	engine->RegisterGlobalFunction("string b_str()", asFUNCTION(b_str), asCALL_CDECL);

	engine->RegisterGlobalFunction("int &b_intref()", asFUNCTION(b_intref), asCALL_CDECL);
	engine->RegisterGlobalFunction("string &b_strref()", asFUNCTION(b_strref), asCALL_CDECL);

	engine->RegisterGlobalFunction("string &complex(string &out)", asFUNCTION(complex), asCALL_CDECL);
	engine->RegisterGlobalFunction("string &complex2()", asFUNCTION(complex2), asCALL_CDECL);
	engine->RegisterGlobalFunction("int &complex3(string &out)", asFUNCTION(complex3), asCALL_CDECL);

	string str;
	engine->RegisterGlobalProperty("string str", &str);

	engine->RegisterObjectType("prop", sizeof(CProp), asOBJ_REF);
	engine->RegisterObjectBehaviour("prop", asBEHAVE_ADDREF, "void f()", asMETHOD(CProp, AddRef), asCALL_THISCALL);
	engine->RegisterObjectBehaviour("prop", asBEHAVE_RELEASE, "void f()", asMETHOD(CProp, Release), asCALL_THISCALL);
	engine->RegisterObjectMethod("prop", "void Get(string &out)", asMETHOD(CProp,Get), asCALL_THISCALL);
	engine->RegisterGlobalFunction("prop @GetProp(string &in)", asFUNCTION(GetProp), asCALL_CDECL);

	COutStream out;

	asIScriptModule *mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
	mod->AddScriptSection("1", script1);
	mod->AddScriptSection("2", script2);
	mod->AddScriptSection("3", script3);	
	mod->AddScriptSection("4", script4);	
	mod->AddScriptSection("5", script5);
	engine->SetMessageCallback(asMETHOD(COutStream,Callback), &out, asCALL_THISCALL);
	mod->Build();

	
	// Verify order of calculations
 	output = "";
	ExecuteString(engine, "a_str() + b_str()", mod);
	if( output != "ba" ) fail = true;

	output = "";
	ExecuteString(engine, "b_strref() = a_str()", mod);
	if( output != "ab" ) fail = true;

	output = "";
	ExecuteString(engine, "b_strref() += a_str()", mod);
	if( output != "ab" ) fail = true;

	output = "";
	ExecuteString(engine, "a_int() + b_int()", mod);
	if( output != "ab" ) fail = true;

	output = "";
	ExecuteString(engine, "b_intref() = a_int()", mod);
	if( output != "ab" ) fail = true;

	output = "";
	ExecuteString(engine, "b_intref() += a_int()", mod);
	if( output != "ab" ) fail = true;

	// Nested output parameters with a returned reference
	ci = 0; cs = ""; str = "";
	ExecuteString(engine, "complex3(complex(str)) = 1", mod);
	if( ci != 1 ) fail = true;
	if( cs != "outparm3" ) fail = true;
	if( str != "outparm" ) fail = true;

	str = "";
 	ExecuteString(engine, "GetProp(\"test\").Get(str);", mod);
	if( str != "PropOut" ) fail = true;

 	engine->Release();


	if( fail )
		printf("%s: fail\n", TESTNAME);

	// Success
	return fail;
}

} // namespace

