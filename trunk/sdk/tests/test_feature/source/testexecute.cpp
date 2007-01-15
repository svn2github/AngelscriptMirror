//
// Tests calling of a c-function from a script
//
// Test author: Fredrik Ehnbom
//

#include "utils.h"

#define TESTNAME "TestExecute"

static bool called = false;

static void cfunction() {
	called = true;
}

bool TestExecute()
{
	bool ret = false;

	asIScriptEngine *engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
	if( strstr(asGetLibraryOptions(),"AS_MAX_PORTABILITY") )
	{
		engine->RegisterGlobalFunction("void cfunction()", asFUNCTION(cfunction), asCALL_GENERIC);
	}
	else
	{
		engine->RegisterGlobalFunction("void cfunction()", asFUNCTION(cfunction), asCALL_CDECL);
	}
	engine->ExecuteString(0, "cfunction()");

	if (!called) {
		printf("\n%s: cfunction not called from script\n\n", TESTNAME);
		ret = true;
	}

	
	asIScriptContext *ctx = engine->CreateContext();
	assert(ctx->SetUserData((void*)0xDEADF00D) == 0);
	assert(ctx->GetUserData() == (void*)0xDEADF00D);
	assert(ctx->SetUserData(0) == (void*)0xDEADF00D);
	ctx->Release();

	engine->Release();
	engine = NULL;

	return ret;
}
