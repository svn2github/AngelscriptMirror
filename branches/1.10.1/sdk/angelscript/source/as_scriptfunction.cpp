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
// as_scriptfunction.cpp
//
// A container for a compiled script function
//



#include "as_config.h"
#include "as_scriptfunction.h"
#include "as_tokendef.h"
#include "as_scriptengine.h"


int asCScriptFunction::GetSpaceNeededForArguments()
{
	// We need to check the size for each type
	int s = 0;
	for( int n = 0; n < parameterTypes.GetLength(); n++ )
		s += parameterTypes[n].GetSizeOnStackDWords();

	return s;
}

int asCScriptFunction::GetSpaceNeededForReturnValue()
{
	return returnType.GetSizeOnStackDWords();
}

asCString asCScriptFunction::GetDeclaration(asCScriptEngine *engine)
{
	asCString str;

	str = returnType.Format();
	str += " " + name + "(";

	if( parameterTypes.GetLength() > 0 )
	{
		int n;
		for( n = 0; n < parameterTypes.GetLength() - 1; n++ )
			str += parameterTypes[n].Format() + ", ";

		str += parameterTypes[n].Format();
	}

	str += ")";

	return str;
}

int asCScriptFunction::GetLineNumber(int programPosition)
{
	if( lineNumbers.GetLength() == 0 ) return 0;

	// Do a binary search in the buffer
	int max = lineNumbers.GetLength()/2 - 1;
	int min = 0;
	int i = max/2;

	for(;;)
	{
		if( lineNumbers[i*2] < programPosition )
		{
			// Have we found the largest number < programPosition?
			if( min == i ) return lineNumbers[i*2+1];

			min = i;
			i = (max + min)/2;
		}
		else if( lineNumbers[i*2] > programPosition )
		{
			// Have we found the smallest number > programPoisition?
			if( max == i ) return lineNumbers[i*2+1];

			max = i;
			i = (max + min)/2;
		}
		else
		{
			// We found the exact position
			return lineNumbers[i*2+1];
		}
	}
}

int asCScriptFunction::GetExceptionID(int programPosition)
{
	if( exceptionIDs.GetLength() == 0 ) return 0;

	// eid 0 isn't stored in the array
	if( programPosition < exceptionIDs[0] ) return 0;

	// Do a binary search in the buffer
	int max = exceptionIDs.GetLength()/2 - 1;
	int min = 0;
	int i = max/2;

	for(;;)
	{
		if( exceptionIDs[i*2] < programPosition )
		{
			// Have we found the largest number < programPosition?
			if( min == i ) return exceptionIDs[i*2+1];

			min = i;
			i = (max + min)/2;
		}
		else if( exceptionIDs[i*2] > programPosition )
		{
			// Have we found the smallest number > programPoisition?
			if( max == i ) return exceptionIDs[i*2+1];

			max = i;
			i = (max + min)/2;
		}
		else
		{
			// We found the exact position
			return exceptionIDs[i*2+1];
		}
	}
}

