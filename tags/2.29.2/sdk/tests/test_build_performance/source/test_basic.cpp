//
// Test author: Andreas Jonsson
//

#include "utils.h"
#include "memory_stream.h"
#include <string>
using std::string;

namespace TestBasic
{

#define TESTNAME "TestBasic"

static const char *scriptBegin =
"void main()                                                 \n"
"{                                                           \n"
"   int[] arr(2);                                            \n"
"   int[][] PWToGuild(26);                                   \n"
"   string test;                                             \n";

static const char *scriptMiddle = 
"   arr[0] = 121; arr[1] = 196; PWToGuild[0] = arr; test = '%d';  \n";

static const char *scriptEnd =
"}                                                           \n";


void Test()
{
	printf("---------------------------------------------\n");
	printf("%s\n\n", TESTNAME);
	printf("AngelScript 2.25.1 WIP 0: 5.01 secs\n");
	printf("AngelScript 2.25.1 WIP 1: 4.86 secs (local bytecode optimizations)\n");
	printf("AngelScript 2.25.1 WIP 2: 4.68 secs (reversed order)\n");
	printf("AngelScript 2.28.1 WIP:   3.42 secs\n");
	printf("AngelScript 2.28.2 WIP:   3.50 secs\n");

 	asIScriptEngine *engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);

	COutStream out;
	engine->SetMessageCallback(asMETHOD(COutStream,Callback), &out, asCALL_THISCALL);

	RegisterScriptArray(engine, true);
	RegisterStdString(engine);

	////////////////////////////////////////////
	printf("\nGenerating...\n");

#ifdef _DEBUG
	const int numLines = 40;
#else
	const int numLines = 40000;
#endif

	string script;
	script.reserve(strlen(scriptBegin) + numLines*(strlen(scriptMiddle)+5) + strlen(scriptEnd));
	script += scriptBegin;
	for( int n = 0; n < numLines; n++ )
	{
		char buf[500];
		sprintf(buf, scriptMiddle, n);
		script += buf;
	}
	script += scriptEnd;

	////////////////////////////////////////////
	printf("\nBuilding...\n");

	double time = GetSystemTimer();

	asIScriptModule *mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
	mod->AddScriptSection(TESTNAME, script.c_str(), script.size(), 0);
	int r = mod->Build();

	time = GetSystemTimer() - time;

	if( r != 0 )
		printf("Build failed\n", TESTNAME);
	else
		printf("Time = %f secs\n", time);

	////////////////////////////////////////////
	printf("\nSaving...\n");

	time = GetSystemTimer();

	CBytecodeStream stream("");
	mod->SaveByteCode(&stream);

	time = GetSystemTimer() - time;
	printf("Time = %f secs\n", time);
	printf("Size = %d\n", int(stream.buffer.size()));

	////////////////////////////////////////////
	printf("\nLoading...\n");

	time = GetSystemTimer();

	asIScriptModule *mod2 = engine->GetModule(0, asGM_ALWAYS_CREATE);
	mod2->LoadByteCode(&stream);

	time = GetSystemTimer() - time;
	printf("Time = %f secs\n", time);

	engine->Release();
}

} // namespace



