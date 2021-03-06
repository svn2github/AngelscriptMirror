/*
   AngelCode Scripting Library
   Copyright (c) 2003-2005 Andreas J�nsson

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
// as_variablescope.cpp
//
// A manager class for variable declarations
//


#include "as_config.h"
#include "as_variablescope.h"

asCVariableScope::asCVariableScope(asCVariableScope *parent)
{
	this->parent    = parent;
	isBreakScope    = false;
	isContinueScope = false;
}

asCVariableScope::~asCVariableScope()
{
	for( int n = 0; n < variables.GetLength(); n++ )
	{
		if( variables[n] ) delete variables[n];

		variables[n] = 0;
	}
}

int asCVariableScope::DeclareVariable(const char *name, const asCDataType &type, int stackOffset)
{
	// TODO: Improve linear search
	// See if the variable is already declared
	if( strcmp(name, "") != 0 )
	{
		for( int n = 0; n < variables.GetLength(); n++ )
		{
			if( variables[n]->name == name )
				return -1;
		}
	}

	sVariable *var = new sVariable;
	var->name = name;
	var->type = type;
	var->stackOffset = stackOffset;
	var->isInitialized = false;

	// Parameters are initialized
	if( stackOffset <= 0 )
		var->isInitialized = true;

	variables.PushLast(var);

	return 0;
}

sVariable *asCVariableScope::GetVariable(const char *name)
{
	// TODO: Improve linear search
	// Find the variable
	for( int n = 0; n < variables.GetLength(); n++ )
	{
		if( variables[n]->name == name )
			return variables[n];
	}

	if( parent )
		return parent->GetVariable(name);

	return 0;
}

sVariable *asCVariableScope::GetVariableByOffset(int offset)
{
	// TODO: Improve linear search
	// Find the variable
	for( int n = 0; n < variables.GetLength(); n++ )
	{
		if( variables[n]->stackOffset == offset )
			return variables[n];
	}

	if( parent )
		return parent->GetVariableByOffset(offset);

	return 0;
}

