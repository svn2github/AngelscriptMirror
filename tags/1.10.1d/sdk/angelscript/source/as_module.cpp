/*
   AngelCode Scripting Library
   Copyright (c) 2003-2004 Andreas J�nsson

   This software is provided 'as-is', without any express or implied 
   warranty. In no event will the authors be held liable for any 
   damages arising from the use of this software.

   Permission is granted to anyone to use this software for any 
   purpose, including commercial applications, and to alter it and 
   redistribute it freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you 
      must not claim that you wrote the original software. If you use
	  this software in a product, an acknowledgment in the product 
	  documentation would be appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and 
      must not be misrepresented as being the original software.

   3. This notice may not be removed or altered from any source 
      distribution.

   The original version of this library can be located at:
   http://www.angelcode.com/angelscript/

   Andreas J�nsson
   andreas@angelcode.com
*/



//
// as_module.cpp
//
// A class that holds a script module
//

#include "as_config.h"
#include "as_module.h"
#include "as_builder.h"
#include "as_context.h"


asCModule::asCModule(const char *name, int id, asCScriptEngine *engine)
{
	this->name     = name;
	this->engine   = engine;
	this->moduleID = id;

	builder = 0;
	isDiscarded = false;
	isBuildWithoutErrors = false;
	contextCount = 0;
	moduleCount = 0;
	isGlobalVarInitialized = false;
}

asCModule::~asCModule()
{
	Reset();

	if( builder ) 
	{
		delete builder;
		builder = 0;
	}

	// Remove the module from the engine
	if( engine )
	{
		if( engine->lastModule == this )
			engine->lastModule = 0;

		int index = (moduleID >> 16);
		engine->scriptModules[index] = 0;
	}
}

int asCModule::AddScriptSection(const char *name, const char *code, int codeLength, int lineOffset)
{
	if( !builder )
		builder = new asCBuilder(engine, this);

	builder->AddCode(name, code, codeLength, lineOffset);

	return asSUCCESS;
}

int asCModule::Build(asIOutputStream *out)
{
	assert( contextCount == 0 );

 	Reset();

	if( !builder )
		return asSUCCESS;

	builder->SetOutputStream(out);

	// Compile the script
	int r = builder->Build();
	delete builder;
	builder = 0;
	
	if( r < 0 )
	{
		// Reset module again
		Reset();

		isBuildWithoutErrors = false;
		return r;
	}

	isBuildWithoutErrors = true;

	engine->PrepareEngine();

	CallInit();

	return r;
}

void asCModule::CallInit()
{
	if( isGlobalVarInitialized ) return;

	if( initFunction.byteCode.GetLength() == 0 ) return;

	int id = moduleID | asFUNC_INIT;
	asIScriptContext *ctx = 0;
	int r = engine->CreateContext(&ctx, true);
	if( r >= 0 && ctx )
	{
		// TODO: Add error handling
		((asCContext*)ctx)->PrepareSpecial(id);
		ctx->Execute();
		ctx->Release();
		ctx = 0;
	}

	isGlobalVarInitialized = true;
}

void asCModule::CallExit()
{
	if( !isGlobalVarInitialized ) return;

	if( exitFunction.byteCode.GetLength() == 0 ) return;

	int id = moduleID | asFUNC_EXIT;
	asIScriptContext *ctx = 0;
	int r = engine->CreateContext(&ctx, true);
	if( r >= 0 && ctx )
	{
		// TODO: Add error handling
		((asCContext*)ctx)->PrepareSpecial(id);
		ctx->Execute();
		ctx->Release();
		ctx = 0;
	}

	isGlobalVarInitialized = false;
}

void asCModule::Discard()
{
	isDiscarded = true;
}

void asCModule::Reset()
{
	assert( !IsUsed() );

	CallExit();

	initFunction.byteCode.SetLength(0);
	exitFunction.byteCode.SetLength(0);

	// Free global variables
	globalMem.SetLength(0);

	isBuildWithoutErrors = true;
	isDiscarded = false;

	int n;
	for( n = 0; n < scriptFunctions.GetLength(); n++ )
		delete scriptFunctions[n];
	scriptFunctions.SetLength(0);

	for( n = 0; n < importedFunctions.GetLength(); n++ )
		delete importedFunctions[n];
	importedFunctions.SetLength(0);

	// Release bound functions
	for( n = 0; n < bindInformations.GetLength(); n++ )
	{
		int oldFuncID = bindInformations[n].importedFunction;
		if( oldFuncID != -1 )
		{
			asCModule *oldModule = engine->GetModule(oldFuncID);
			if( oldModule != 0 ) 
			{
				// Release reference to the module
				oldModule->ReleaseModuleRef();
			}
		}
	}
	bindInformations.SetLength(0);

	for( n = 0; n < stringConstants.GetLength(); n++ )
		asBStrFree(stringConstants[n]);
	stringConstants.SetLength(0);

	for( n = 0; n < scriptGlobals.GetLength(); n++ )
		delete scriptGlobals[n];
	scriptGlobals.SetLength(0);
}

int asCModule::GetFunctionIDByName(const char *name)
{
	if( isBuildWithoutErrors == false )
		return asERROR;
	
	// TODO: Improve linear search
	// Find the function id
	int id = -1;
	for( int n = 0; n < scriptFunctions.GetLength(); n++ )
	{
		if( scriptFunctions[n]->name == name )
		{
			if( id == -1 )
				id = n;
			else
				return asMULTIPLE_FUNCTIONS;
		}
	}

	if( id == -1 ) return asNO_FUNCTION;

	return moduleID | id;
}

int asCModule::GetImportedFunctionCount()
{
	if( isBuildWithoutErrors == false )
		return asERROR;

	return importedFunctions.GetLength();
}

int asCModule::GetImportedFunctionIndexByDecl(const char *decl)
{
	if( isBuildWithoutErrors == false )
		return asERROR;

	asCBuilder bld(engine, this);

	asCScriptFunction func;
	bld.ParseFunctionDeclaration(decl, &func);

	// TODO: Improve linear search
	// Search script functions for matching interface
	int id = -1;
	for( int n = 0; n < importedFunctions.GetLength(); ++n )
	{
		if( func.name == importedFunctions[n]->name && 
			func.returnType == importedFunctions[n]->returnType &&
			func.parameterTypes.GetLength() == importedFunctions[n]->parameterTypes.GetLength() )
		{
			bool match = true;
			for( int p = 0; p < func.parameterTypes.GetLength(); ++p )
			{
				if( func.parameterTypes[p] != importedFunctions[n]->parameterTypes[p] )
				{
					match = false;
					break;
				}
			}

			if( match )
			{
				if( id == -1 )
					id = n;
				else
					return asMULTIPLE_FUNCTIONS;
			}
		}
	}

	if( id == -1 ) return asNO_FUNCTION;

	return id;
}

int asCModule::GetFunctionCount()
{
	if( isBuildWithoutErrors == false )
		return asERROR;

	return scriptFunctions.GetLength();
}

int asCModule::GetFunctionIDByDecl(const char *decl)
{
	if( isBuildWithoutErrors == false )
		return asERROR;

	asCBuilder bld(engine, this);

	asCScriptFunction func;
	bld.ParseFunctionDeclaration(decl, &func);

	// TODO: Improve linear search
	// Search script functions for matching interface
	int id = -1;
	for( int n = 0; n < scriptFunctions.GetLength(); ++n )
	{
		if( func.name == scriptFunctions[n]->name && 
			func.returnType == scriptFunctions[n]->returnType &&
			func.parameterTypes.GetLength() == scriptFunctions[n]->parameterTypes.GetLength() )
		{
			bool match = true;
			for( int p = 0; p < func.parameterTypes.GetLength(); ++p )
			{
				if( func.parameterTypes[p] != scriptFunctions[n]->parameterTypes[p] )
				{
					match = false;
					break;
				}
			}

			if( match )
			{
				if( id == -1 )
					id = n;
				else
					return asMULTIPLE_FUNCTIONS;
			}
		}
	}

	if( id == -1 ) return asNO_FUNCTION;

	return moduleID | id;
}

int asCModule::GetGlobalVarCount()
{
	if( isBuildWithoutErrors == false )
		return asERROR;

	return scriptGlobals.GetLength();
}

int asCModule::GetGlobalVarIDByName(const char *name)
{
	if( isBuildWithoutErrors == false )
		return asERROR;

	// Find the global var id
	int id = -1;
	for( int n = 0; n < scriptGlobals.GetLength(); n++ )
	{
		if( scriptGlobals[n]->name == name )
		{
			id = n;
			break;
		}
	}

	if( id == -1 ) return asNO_GLOBAL_VAR;

	return moduleID | id;
}

int asCModule::GetGlobalVarIDByDecl(const char *decl)
{
	if( isBuildWithoutErrors == false )
		return asERROR;

	asCBuilder bld(engine, this);

	asCProperty gvar;
	bld.ParseVariableDeclaration(decl, &gvar);

	// TODO: Improve linear search
	// Search script functions for matching interface
	int id = -1;
	for( int n = 0; n < scriptGlobals.GetLength(); ++n )
	{
		if( gvar.name == scriptGlobals[n]->name && 
			gvar.type == scriptGlobals[n]->type )
		{
			id = n;
			break;
		}
	}

	if( id == -1 ) return asNO_GLOBAL_VAR;

	return moduleID | id;
}

int asCModule::AddConstantBStr(const char *str, int len)
{
	// TODO: Improve linear search
	// Has the string been registered before?
	for( int n = 0; n < stringConstants.GetLength(); n++ )
	{
		if( asBStrLength(stringConstants[n]) == (unsigned)len &&
			memcmp(str, stringConstants[n], len) == 0 )
			return n;
	}

	// No match was found, add the string
	asBSTR bstr = asBStrAlloc((unsigned)len);
	memcpy(bstr, str, len);
	stringConstants.PushLast(bstr);

	return stringConstants.GetLength() - 1;
}

asBSTR *asCModule::GetConstantBStr(int id)
{
	return &stringConstants[id];
}

int asCModule::GetNextFunctionId()
{
	return scriptFunctions.GetLength();
}

int asCModule::GetNextImportedFunctionId()
{
	return FUNC_IMPORTED | importedFunctions.GetLength();
}

int asCModule::AddScriptFunction(int id, const char *name, const asCDataType &returnType, asCDataType *params, int paramCount)
{
	assert(id >= 0);

	// Store the function information
	asCScriptFunction *func = new asCScriptFunction();
	func->name       = name;
	func->id         = id;
	func->returnType = returnType;
	for( int n = 0; n < paramCount; n++ )
		func->parameterTypes.PushLast(params[n]);
	func->objectType = 0;

	scriptFunctions.PushLast(func);

	return 0;
}

int asCModule::AddImportedFunction(int id, const char *name, const asCDataType &returnType, asCDataType *params, int paramCount, int moduleNameStringID)
{
	assert(id >= 0);

	// Store the function information
	asCScriptFunction *func = new asCScriptFunction();
	func->name       = name;
	func->id         = id;
	func->returnType = returnType;
	for( int n = 0; n < paramCount; n++ )
		func->parameterTypes.PushLast(params[n]);
	func->objectType = 0;

	importedFunctions.PushLast(func);

	sBindInfo info;
	info.importedFunction = -1;
	info.importFrom = moduleNameStringID;
	bindInformations.PushLast(info);

	return 0;
}

asCScriptFunction *asCModule::GetScriptFunction(int funcID)
{
	return scriptFunctions[funcID & 0xFFFF];
}

asCScriptFunction *asCModule::GetImportedFunction(int funcID)
{
	return importedFunctions[funcID & 0xFFFF];
}

asCScriptFunction *asCModule::GetSpecialFunction(int funcID)
{
	if( funcID & FUNC_IMPORTED )
		return importedFunctions[funcID & 0xFFFF];
	else
	{
		if( (funcID & 0xFFFF) == asFUNC_INIT )
			return &initFunction;
		else if( (funcID & 0xFFFF) == asFUNC_EXIT )
			return &exitFunction;
		else if( (funcID & 0xFFFF) == asFUNC_STRING )
		{
			assert(false);
		}

		return scriptFunctions[funcID & 0xFFFF];
	}
}

int asCModule::AllocGlobalMemory(int size)
{
	int index = globalMem.GetLength();

	globalMem.SetLength(index + size);

	return index;
}

int asCModule::AddContextRef()
{
	ENTERCRITICALSECTION(criticalSection);
	int r = ++contextCount;
	LEAVECRITICALSECTION(criticalSection);
	return r;
}

int asCModule::ReleaseContextRef()
{
	ENTERCRITICALSECTION(criticalSection);
	int r = --contextCount;
	LEAVECRITICALSECTION(criticalSection);

	return r;
}

int asCModule::AddModuleRef()
{
	ENTERCRITICALSECTION(criticalSection);
	int r = ++moduleCount;
	LEAVECRITICALSECTION(criticalSection);
	return r;
}

int asCModule::ReleaseModuleRef()
{
	ENTERCRITICALSECTION(criticalSection);
	int r = --moduleCount;
	LEAVECRITICALSECTION(criticalSection);

	return r;
}

bool asCModule::CanDelete()
{
	// Don't delete if not discarded
	if( !isDiscarded ) return false;

	// Are there any contexts still referencing the module?
	if( contextCount ) return false;

	// If there are modules referencing this one we need to check for circular referencing
	if( moduleCount )
	{
		// Check if all the modules are without external reference
		asCArray<asCModule*> modules;
		if( CanDeleteAllReferences(modules) )
		{
			// Unbind all functions. This will break any circular references
			for( int n = 0; n < bindInformations.GetLength(); n++ )
			{
				int oldFuncID = bindInformations[n].importedFunction;
				if( oldFuncID != -1 )
				{
					asCModule *oldModule = engine->GetModule(oldFuncID);
					if( oldModule != 0 ) 
					{
						// Release reference to the module
						oldModule->ReleaseModuleRef();
					}
				}
			}
		}

		// Can't delete the module yet because the  
		// other modules haven't released this one
		return false;
	}

	return true;
}

bool asCModule::CanDeleteAllReferences(asCArray<asCModule*> &modules)
{
	if( !isDiscarded ) return false;

	if( contextCount ) return false;

	modules.PushLast(this);

	// Check all bound functions for referenced modules
	for( int n = 0; n < bindInformations.GetLength(); n++ )
	{
		int funcID = bindInformations[n].importedFunction;
		asCModule *module = engine->GetModule(funcID);

		// If the module is already in the list don't check it again
		bool inList = false;
		for( int m = 0; m < modules.GetLength(); m++ )
		{
			if( modules[m] == module )
			{
				inList = true;
				break;
			}
		}

		if( !inList )
		{
			bool ret = module->CanDeleteAllReferences(modules);
			if( ret == false ) return false;
		}
	}

	// If no module has external references then all can be deleted
	return true;
}


int asCModule::BindImportedFunction(int index, int sourceID)
{
	// Remove reference to old module
	int oldFuncID = bindInformations[index].importedFunction;
	if( oldFuncID != -1 )
	{
		asCModule *oldModule = engine->GetModule(oldFuncID);
		if( oldModule != 0 ) 
		{
			// Release reference to the module
			oldModule->ReleaseModuleRef();
		}
	}

	if( sourceID == -1 )
	{
		bindInformations[index].importedFunction = -1;
		return asSUCCESS;
	}

	// Must verify that the interfaces are equal
	asCModule *srcModule = engine->GetModule(sourceID);
	if( srcModule == 0 ) return asNO_MODULE;

	asCScriptFunction *dst = GetImportedFunction(index);
	if( dst == 0 ) return asNO_FUNCTION;

	asCScriptFunction *src = srcModule->GetScriptFunction(sourceID);
	if( src == 0 ) return asNO_FUNCTION;

	// Verify return type
	if( dst->returnType != src->returnType )
		return asINVALID_INTERFACE;

	if( dst->parameterTypes.GetLength() != src->parameterTypes.GetLength() )
		return asINVALID_INTERFACE;

	for( int n = 0; n < dst->parameterTypes.GetLength(); ++n )
	{
		if( dst->parameterTypes[n] != src->parameterTypes[n] )
			return asINVALID_INTERFACE;
	}

	// Add reference to new module
	srcModule->AddModuleRef();

	bindInformations[index].importedFunction = sourceID;

	return asSUCCESS;
}

const char *asCModule::GetImportedFunctionSourceModule(int index)
{
	if( index >= bindInformations.GetLength() )
		return 0;

	index = bindInformations[index].importFrom;

	return (const char *)stringConstants[index];
}

bool asCModule::IsUsed()
{
	if( contextCount ) return true;
	if( moduleCount ) return true;

	return false;
}
