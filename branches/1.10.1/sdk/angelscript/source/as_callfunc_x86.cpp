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
// as_callfunc.cpp
//
// These functions handle the actual calling of system functions
//


#include "as_config.h"
#include "as_callfunc.h"
#include "as_scriptengine.h"
#include "as_texts.h"
#include "as_tokendef.h"


int DetectCallingConvention(const char *obj, asUPtr &ptr, int callConv, asSSystemFunctionInterface *internal)
{
	memset(internal, 0, sizeof(asSSystemFunctionInterface));

	internal->func = (asDWORD)ptr.func;

	int base = callConv;
	if( obj == 0 )
	{
		if( base == asCALL_CDECL )
			internal->callConv = ICC_CDECL;
		else if( base == asCALL_STDCALL )
			internal->callConv = ICC_STDCALL;
		else
			return asNOT_SUPPORTED;
	}
	else
	{
		if( base == asCALL_THISCALL )
		{
			internal->callConv = ICC_THISCALL;
#ifdef GNU_STYLE_VIRTUAL_METHOD
			if( (asDWORD(ptr.func) & 1) )
				internal->callConv = ICC_VIRTUAL_THISCALL;
#endif
			internal->baseOffset = MULTI_BASE_OFFSET(ptr);

#ifdef HAVE_VIRTUAL_BASE_OFFSET
			// We don't support virtual inheritance
			if( VIRTUAL_BASE_OFFSET(ptr) != 0 )
				return asNOT_SUPPORTED;
#endif
		}
		else if( base == asCALL_CDECL_OBJLAST )
			internal->callConv = ICC_CDECL_OBJLAST;
        else if( base == asCALL_CDECL_OBJFIRST )
            internal->callConv = ICC_CDECL_OBJFIRST;
		else
			return asNOT_SUPPORTED;
	}

	return 0;
}

// This function should prepare system functions so that it will be faster to call them
int PrepareSystemFunction(asCScriptFunction *func, asSSystemFunctionInterface *internal, asCScriptEngine *engine)
{
	// References and pointers are always returned the same way
	if( func->returnType.isReference || func->returnType.pointerLevel > 0 )
	{
		internal->hostReturnInMemory = false;
		internal->hostReturnSize = 1;
		internal->hostReturnFloat = false;
	}
	// Registered types have special flags that determine how they are returned
	else if( func->returnType.objectType != 0 )
	{
		asDWORD objType = func->returnType.objectType->flags;
		if( objType == 0 )
		{
			objType = asOBJ_CLASS;

			// We should determine type from registered behaviours
			asSTypeBehaviour *beh = engine->GetBehaviour(&func->returnType);
			if( beh )
			{
				if( beh->construct ) objType |= asOBJ_CLASS_CONSTRUCTOR;
				if( beh->destruct  ) objType |= asOBJ_CLASS_DESTRUCTOR;
				if( beh->copy      ) objType |= asOBJ_CLASS_ASSIGNMENT;
			}
		}

		if( objType & asOBJ_CLASS )
		{
			if( objType & COMPLEX_MASK )
			{
				internal->hostReturnInMemory = true;
				internal->hostReturnSize = 1;
				internal->hostReturnFloat = false;
			}
			else
			{
				internal->hostReturnFloat = false;
				if( func->returnType.GetSizeOnStackDWords() > 2 )
				{
					internal->hostReturnInMemory = true;
					internal->hostReturnSize = 1;
				}
				else
				{
					internal->hostReturnInMemory = false;
					internal->hostReturnSize = func->returnType.GetSizeOnStackDWords();
				}

#ifdef THISCALL_RETURN_SIMPLE_IN_MEMORY
				if( internal->callConv == ICC_THISCALL || 
					internal->callConv == ICC_VIRTUAL_THISCALL )
				{
					internal->hostReturnInMemory = true;
					internal->hostReturnSize = 1;
				}
#endif
#ifdef CDECL_RETURN_SIMPLE_IN_MEMORY
				if( internal->callConv == ICC_CDECL || 
					internal->callConv == ICC_CDECL_OBJLAST ||
					internal->callConv == ICC_CDECL_OBJFIRST )
				{
					internal->hostReturnInMemory = true;
					internal->hostReturnSize = 1;
				}
#endif
#ifdef STDCALL_RETURN_SIMPLE_IN_MEMORY
				if( internal->callConv == ICC_STDCALL )
				{
					internal->hostReturnInMemory = true;
					internal->hostReturnSize = 1;
				}
#endif
			}
		}
		else if( objType == asOBJ_PRIMITIVE )
		{
			internal->hostReturnInMemory = false;
			internal->hostReturnSize = func->returnType.GetSizeOnStackDWords();
			internal->hostReturnFloat = false;
		}
		else if( objType == asOBJ_FLOAT )
		{
			internal->hostReturnInMemory = false;
			internal->hostReturnSize = func->returnType.GetSizeOnStackDWords();
			internal->hostReturnFloat = true;
		}
	}
	// Primitive types can easily be determined
	else if( func->returnType.GetSizeOnStackDWords() > 2 )
	{
		internal->hostReturnInMemory = true;
		internal->hostReturnSize = 1; 
		internal->hostReturnFloat = false;
	}
	else if( func->returnType.GetSizeOnStackDWords() == 2 )
	{
		internal->hostReturnInMemory = false;
		internal->hostReturnSize = 2;
		internal->hostReturnFloat = func->returnType.IsEqualExceptConst(asCDataType(ttDouble, true, false));
	}
	else if( func->returnType.GetSizeOnStackDWords() == 1 )
	{
		internal->hostReturnInMemory = false;
		internal->hostReturnSize = 1;
		internal->hostReturnFloat = func->returnType.IsEqualExceptConst(asCDataType(ttFloat, true, false));
	}
	else
	{
		internal->hostReturnInMemory = false;
		internal->hostReturnSize = 0;
		internal->hostReturnFloat = false;
	}

	// Calculate the size needed for the parameters
	internal->paramSize = func->GetSpaceNeededForArguments();

	return 0;
}

typedef asQWORD (*t_CallCDeclQW)(const asDWORD *, int, asDWORD); 
typedef asDWORD (*t_CallCDeclRetByRef)(const asDWORD *, int, asDWORD, void *); 
typedef asQWORD (*t_CallSTDCallQW)(const asDWORD *, int, asDWORD); 
typedef asQWORD (*t_CallThisCallQW)(const void *, const asDWORD *, int, asDWORD); 
typedef asDWORD (*t_CallThisCallRetByRef)(const void *, const asDWORD *, int, asDWORD, void *);

// Prototypes
void CallCDeclFunction(const asDWORD *args, int paramSize, asDWORD func);
void CallCDeclFunctionObjFirst(const asDWORD *args, int paramSize, asDWORD func);
void CallCDeclFunctionRetByRef_impl(const asDWORD *args, int paramSize, asDWORD func, void *retPtr);
void CallCDeclFunctionRetByRefObjFirst_impl(const asDWORD *args, int paramSize, asDWORD func, void *retPtr);
void CallSTDCallFunction(const asDWORD *args, int paramSize, asDWORD func);
void CallThisCallFunction(const void *obj, const asDWORD *args, int paramSize, asDWORD func);
void CallThisCallFunctionRetByRef_impl(const void *, const asDWORD *, int, asDWORD, void *retPtr); 

// Initialize function pointers
const t_CallCDeclQW CallCDeclFunctionQWord = (t_CallCDeclQW)CallCDeclFunction;
const t_CallCDeclQW CallCDeclFunctionQWordObjFirst = (t_CallCDeclQW)CallCDeclFunctionObjFirst;
const t_CallCDeclRetByRef CallCDeclFunctionRetByRef = (t_CallCDeclRetByRef)CallCDeclFunctionRetByRef_impl;
const t_CallCDeclRetByRef CallCDeclFunctionRetByRefObjFirst = (t_CallCDeclRetByRef)CallCDeclFunctionRetByRefObjFirst_impl;
const t_CallSTDCallQW CallSTDCallFunctionQWord = (t_CallSTDCallQW)CallSTDCallFunction;
const t_CallThisCallQW CallThisCallFunctionQWord = (t_CallThisCallQW)CallThisCallFunction;
const t_CallThisCallRetByRef CallThisCallFunctionRetByRef = (t_CallThisCallRetByRef)CallThisCallFunctionRetByRef_impl;

asDWORD GetReturnedFloat();
asQWORD GetReturnedDouble();

int CallSystemFunction(int id, asCContext *context)
{
	id = -id - 1;

	asQWORD retTemp = 0;
	asDWORD retDW = 0;
	asQWORD retQW = 0;

	asCScriptEngine *engine = context->engine;

	asSSystemFunctionInterface *sysFunc = (engine->systemFunctionInterfaces)[id];

	int     callConv           = sysFunc->callConv;
	void   *func               = (void*)sysFunc->func;
	int     paramSize          = sysFunc->paramSize;

	asDWORD *args              = context->stackPointer;
	void    *retPointer = 0;
	void    *obj = 0;
	asDWORD *vftable;
	int     popSize            = paramSize;

	if( sysFunc->scriptReturnInMemory )
	{
		popSize++;
		if( !sysFunc->hostReturnInMemory )
		{
			// Skip the address sent by the script engine
			args++;
		}
		else
		{
			// The return is made in memory
			callConv++;

			// Get the return pointer
			retPointer = (void*)*args;

			// Skip the pointer
			args++;
		}
	}
	else
	{
		if( sysFunc->hostReturnInMemory )
		{
			// The return is made in memory
			callConv++;

			retPointer = (void*)&retTemp;
		}
	}
	
	if( callConv >= ICC_THISCALL )
	{
		popSize++;
		// Check for null pointer
		obj = (void*)*(args + paramSize);
		if( obj == 0 )
		{	
			context->SetInternalException(TXT_NULL_POINTER_ACCESS);
			return 0;
		}

		// Add the base offset for multiple inheritance
		obj = (void*)(int(obj) + sysFunc->baseOffset);
	}

	context->isCallingSystemFunction = true;
	switch( callConv )
	{
	case ICC_CDECL:
		retQW = CallCDeclFunctionQWord(args, paramSize<<2, (asDWORD)func);
		break;
		
	case ICC_CDECL_RETURNINMEM:
		retQW = CallCDeclFunctionRetByRef(args, paramSize<<2, (asDWORD)func, retPointer);
		break;

	case ICC_STDCALL:
		retQW = CallSTDCallFunctionQWord(args, paramSize<<2, (asDWORD)func);
		break;

	case ICC_STDCALL_RETURNINMEM:
		// Push the return pointer on the stack
		paramSize++;
		args--;
		*args = (asDWORD)retPointer;

		retQW = CallSTDCallFunctionQWord(args, paramSize<<2, (asDWORD)func);
		break;

	case ICC_THISCALL:
		retQW = CallThisCallFunctionQWord(obj, args, paramSize<<2, (asDWORD)func);
		break;

	case ICC_THISCALL_RETURNINMEM:
		retQW = CallThisCallFunctionRetByRef(obj, args, paramSize<<2, (asDWORD)func, retPointer);
		break;

	case ICC_VIRTUAL_THISCALL:
		// Get virtual function table from the object pointer
		vftable = *(asDWORD**)obj;

		retQW = CallThisCallFunctionQWord(obj, args, paramSize<<2, vftable[asDWORD(func)>>2]);
		break;

	case ICC_VIRTUAL_THISCALL_RETURNINMEM:
		// Get virtual function table from the object pointer
		vftable = *(asDWORD**)obj;

		retQW = CallThisCallFunctionRetByRef(obj, args, paramSize<<2, vftable[asDWORD(func)>>2], retPointer);
		break;

	case ICC_CDECL_OBJLAST:
		// Call the system object method as a cdecl with the obj ref as the last parameter
		paramSize++;

		retQW = CallCDeclFunctionQWord(args, paramSize<<2, (asDWORD)func);
		break;

	case ICC_CDECL_OBJLAST_RETURNINMEM:
		// Call the system object method as a cdecl with the obj ref as the last parameter
		paramSize++;

		retQW = CallCDeclFunctionRetByRef(args, paramSize<<2, (asDWORD)func, retPointer);
		break;

    case ICC_CDECL_OBJFIRST:
        // Call the system object method as a cdecl with the obj ref as the first parameter
        retQW = CallCDeclFunctionQWordObjFirst(args, paramSize<<2, (asDWORD)func);
        break;

    case ICC_CDECL_OBJFIRST_RETURNINMEM:
        // Call the system object method as a cdecl with the obj ref as the first parameter
		retQW = CallCDeclFunctionRetByRefObjFirst(args, paramSize<<2, (asDWORD)func, retPointer);
		break;

	default:
		context->SetInternalException(TXT_INVALID_CALLING_CONVENTION);
	}
	context->isCallingSystemFunction = false;

	if( context->status == tsUnhandledException )
		return 0;

	// Restore the original location of the arguments
	args = context->stackPointer;

	// Store the returned value in our stack
	if( sysFunc->scriptReturnInMemory )
	{
		if( !sysFunc->hostReturnInMemory )
		{
			// Copy the returned value to the pointer sent by the script engine
			if( sysFunc->hostReturnSize == 1 )
				*(asDWORD*)*args = (asDWORD)retQW;
			else
				*(asQWORD*)*args = retQW;
		}

		// Return the address to the location
		*(asDWORD*)&context->returnVal = *args;
	}
	else
	{
		if( sysFunc->hostReturnInMemory )
		{
			// Copy the return value
			retDW = (asDWORD)retTemp;
		}
		else
		{
			if( sysFunc->hostReturnFloat )
			{
				if( sysFunc->hostReturnSize == 1 )
					retDW = GetReturnedFloat();
				else
					retQW = GetReturnedDouble();
			}
			else if( sysFunc->hostReturnSize == 1 )
				retDW = (asDWORD)retQW;
		}

		// Store value in returnVal register
		if( sysFunc->hostReturnSize == 1 )
			*(asDWORD*)&context->returnVal = retDW;
		else if( sysFunc->hostReturnSize == 2 )
			context->returnVal = retQW;
	}

	return popSize;
}


void CallCDeclFunction(const asDWORD *args, int paramSize, asDWORD func)
{
#if defined ASM_INTEL

	// Copy the data to the real stack. If we fail to do
	// this we may run into trouble in case of exceptions.
	__asm
	{
		// We must save registers that are used
	    push ecx			

		// Copy arguments from script  
		// stack to application stack
        mov  ecx, paramSize
		mov  eax, args
		add  eax, ecx
		cmp  ecx, 0
		je   endcopy
copyloop:
		sub  eax, 4
		push dword ptr [eax]
		sub  ecx, 4
		jne  copyloop
endcopy:

		// Call function
		call [func]   
			
		// Pop arguments from stack
		add  esp, paramSize

		// Restore registers
		pop  ecx

		// return value in EAX or EAX:EDX
	}

#elif defined ASM_AT_N_T

	asm("pushl %ecx           \n"
		"movl  12(%ebp), %ecx \n" // paramSize
		"movl  8(%ebp), %eax  \n" // args
		"addl  %ecx, %eax     \n" // push arguments on the stack
		"cmp   $0, %ecx       \n"
		"je    endcopy        \n"
		"copyloop:            \n"
		"subl  $4, %eax       \n"
		"pushl (%eax)         \n"
		"subl  $4, %ecx       \n"
		"jne   copyloop       \n"
		"endcopy:             \n"
		"call  *16(%ebp)      \n"
		"addl  12(%ebp), %esp \n" // pop arguments 
		"popl  %ecx           \n");

#endif
}

void CallCDeclFunctionObjFirst(const asDWORD *args, int paramSize, asDWORD func)
{
#if defined ASM_INTEL

	// Copy the data to the real stack. If we fail to do
	// this we may run into trouble in case of exceptions.
	__asm
	{
		// We must save registers that are used
	    push ecx			

		// Copy arguments from script  
		// stack to application stack
        mov  ecx, paramSize
		mov  eax, args
		add  eax, ecx
		cmp  ecx, 0
		je   endcopy
copyloop:
		sub  eax, 4
		push dword ptr [eax]
		sub  ecx, 4
		jne  copyloop
endcopy:

		// push object as first parameter
		mov  eax, args
		add  eax, paramSize
        push dword ptr [eax]        

		// Call function
		call [func]   
			
		// Pop arguments from stack
		add  esp, paramSize
        add  esp, 4

		// Restore registers
		pop  ecx

		// return value in EAX or EAX:EDX
	}

#elif defined ASM_AT_N_T

	asm("pushl %ecx           \n"
		"movl  12(%ebp), %ecx \n" // paramSize
		"movl  8(%ebp), %eax  \n" // args
		"addl  %ecx, %eax     \n" // push arguments on the stack
		"cmp   $0, %ecx       \n"
		"je    endcopy6       \n"
		"copyloop6:           \n"
		"subl  $4, %eax       \n"
		"pushl (%eax)         \n"
		"subl  $4, %ecx       \n"
		"jne   copyloop6      \n"
		"endcopy6:            \n"
		"movl  12(%ebp), %ecx \n" // paramSize
		"movl  8(%ebp), %eax  \n" // args
		"addl  %ecx, %eax     \n" 
        "pushl (%eax)         \n" // push object first
		"call  *16(%ebp)      \n"
		"addl  12(%ebp), %esp \n" // pop arguments 
        "addl  $4, %esp       \n"
		"popl  %ecx           \n");

#endif
}

void CallCDeclFunctionRetByRefObjFirst_impl(const asDWORD *args, int paramSize, asDWORD func, void *retPtr)
{
#if defined ASM_INTEL

	// Copy the data to the real stack. If we fail to do
	// this we may run into trouble in case of exceptions.
	__asm
	{
		// We must save registers that are used
	    push ecx			

		// Copy arguments from script  
		// stack to application stack
        mov  ecx, paramSize
		mov  eax, args
		add  eax, ecx
		cmp  ecx, 0
		je   endcopy
copyloop:
		sub  eax, 4
		push dword ptr [eax]
		sub  ecx, 4
		jne  copyloop
endcopy:

		// Push the object pointer
		mov  eax, args
		add  eax, paramSize
        push dword ptr [eax]    

		// Push the return pointer
		push retPtr;

		// Call function
		call [func]   
			
		// Pop arguments from stack
		add  esp, paramSize

#ifndef CALLEE_POPS_HIDDEN_RETURN_POINTER
		// Pop the return pointer
		add  esp, 8
#else
        add  esp, 4
#endif
		// Restore registers
		pop  ecx

		// return value in EAX or EAX:EDX
	}

#elif defined ASM_AT_N_T

	asm("pushl %ecx           \n"
		"movl  12(%ebp), %ecx \n" // paramSize
		"movl  8(%ebp), %eax  \n" // args
		"addl  %ecx, %eax     \n" // push arguments on the stack
		"cmp   $0, %ecx       \n"
		"je    endcopy5       \n"
		"copyloop5:           \n"
		"subl  $4, %eax       \n"
		"pushl (%eax)         \n"
		"subl  $4, %ecx       \n"
		"jne   copyloop5      \n"
		"endcopy5:            \n"
		"movl  12(%ebp), %ecx \n" // paramSize
		"movl  8(%ebp), %eax  \n" // args
		"addl  %ecx, %eax     \n" 
        "pushl (%eax)         \n" // push object first
		"pushl 20(%ebp)       \n" // retPtr
		"call  *16(%ebp)      \n" // func
		"addl  12(%ebp), %esp \n" // pop arguments 
#ifndef CALLEE_POPS_HIDDEN_RETURN_POINTER
		"addl  $8, %esp       \n" // Pop the return pointer
#else
		"addl  $4, %esp       \n" // Pop the return pointer
#endif
		"popl  %ecx           \n");

#endif
}

void CallCDeclFunctionRetByRef_impl(const asDWORD *args, int paramSize, asDWORD func, void *retPtr)
{
#if defined ASM_INTEL

	// Copy the data to the real stack. If we fail to do
	// this we may run into trouble in case of exceptions.
	__asm
	{
		// We must save registers that are used
	    push ecx			

		// Copy arguments from script  
		// stack to application stack
        mov  ecx, paramSize
		mov  eax, args
		add  eax, ecx
		cmp  ecx, 0
		je   endcopy
copyloop:
		sub  eax, 4
		push dword ptr [eax]
		sub  ecx, 4
		jne  copyloop
endcopy:

		// Push the return pointer
		push retPtr;

		// Call function
		call [func]   
			
		// Pop arguments from stack
		add  esp, paramSize

#ifndef CALLEE_POPS_HIDDEN_RETURN_POINTER
		// Pop the return pointer
		add  esp, 4
#endif
		// Restore registers
		pop  ecx

		// return value in EAX or EAX:EDX
	}

#elif defined ASM_AT_N_T

	asm("pushl %ecx           \n"
		"movl  12(%ebp), %ecx \n" // paramSize
		"movl  8(%ebp), %eax  \n" // args
		"addl  %ecx, %eax     \n" // push arguments on the stack
		"cmp   $0, %ecx       \n"
		"je    endcopy4       \n"
		"copyloop4:           \n"
		"subl  $4, %eax       \n"
		"pushl (%eax)         \n"
		"subl  $4, %ecx       \n"
		"jne   copyloop4      \n"
		"endcopy4:            \n"
		"pushl 20(%ebp)       \n" // retPtr
		"call  *16(%ebp)      \n" // func
		"addl  12(%ebp), %esp \n" // pop arguments 
#ifndef CALLEE_POPS_HIDDEN_RETURN_POINTER
		"addl  $4, %esp       \n" // Pop the return pointer
#endif
		"popl  %ecx           \n");

#endif
}


void CallSTDCallFunction(const asDWORD *args, int paramSize, asDWORD func)
{
#if defined ASM_INTEL

	// Copy the data to the real stack. If we fail to do
	// this we may run into trouble in case of exceptions.
	__asm
	{
		// We must save registers that are used
	    push ecx

		// Copy arguments from script  
		// stack to application stack
        mov  ecx, paramSize
		mov  eax, args
		add  eax, ecx
		cmp  ecx, 0
		je   endcopy
copyloop:
		sub  eax, 4
		push dword ptr [eax]
		sub  ecx, 4
		jne  copyloop
endcopy:

		// Call function
		call [func]   
			
		// The callee already removed parameters from the stack

		// Restore registers
		pop  ecx

		// return value in EAX or EAX:EDX
	}

#elif defined ASM_AT_N_T

	asm("pushl %ecx           \n"
		"movl  12(%ebp), %ecx \n" // paramSize
		"movl  8(%ebp), %eax  \n" // args
		"addl  %ecx, %eax     \n" // push arguments on the stack
		"cmp   $0, %ecx       \n"
		"je    endcopy2       \n"
		"copyloop2:           \n"
		"subl  $4, %eax       \n"
		"pushl (%eax)         \n"
		"subl  $4, %ecx       \n"
		"jne   copyloop2      \n"
		"endcopy2:            \n"
		"call  *16(%ebp)      \n" // callee pops the arguments
		"popl  %ecx           \n");

#endif
}


void CallThisCallFunction(const void *obj, const asDWORD *args, int paramSize, asDWORD func)
{
#if defined ASM_INTEL

	// Copy the data to the real stack. If we fail to do
	// this we may run into trouble in case of exceptions.
	__asm
	{
		// We must save registers that are used
	    push ecx			

		// Copy arguments from script  
		// stack to application stack
        mov  ecx, paramSize
		mov  eax, args
		add  eax, ecx
		cmp  ecx, 0
		je   endcopy
copyloop:
		sub  eax, 4
		push dword ptr [eax]
		sub  ecx, 4
		jne  copyloop
endcopy:

#ifdef THISCALL_PASS_OBJECT_POINTER_ON_THE_STACK
		// Push the object pointer on the stack
		push obj
#else
		// Move object pointer to ECX
		mov  ecx, obj
#endif

		// Call function
		call [func]         

#ifndef THISCALL_CALLEE_POPS_ARGUMENTS
		// Pop arguments
		add  esp, paramSize
#ifdef THISCALL_PASS_OBJECT_POINTER_ON_THE_STACK
		// Pop object pointer
		add esp, 4
#endif
#endif

		// Restore registers
		pop  ecx

		// Return value in EAX or EAX:EDX
	}

#elif defined ASM_AT_N_T

	asm("pushl %ecx           \n"
		"movl  16(%ebp), %ecx \n" // paramSize
		"movl  12(%ebp), %eax \n" // args
		"addl  %ecx, %eax     \n" // push all arguments on the stack
		"cmp   $0, %ecx       \n"
		"je    endcopy1       \n"
		"copyloop1:           \n"
		"subl  $4, %eax       \n"
		"pushl (%eax)         \n"
		"subl  $4, %ecx       \n"
		"jne   copyloop1      \n"
		"endcopy1:            \n"
		"movl  8(%ebp), %ecx  \n" // move obj into ECX
		"pushl 8(%ebp)        \n" // push obj on the stack
		"call  *20(%ebp)      \n"
		"addl  16(%ebp), %esp \n" // pop arguments
		"addl  $4, %esp       \n" // pop obj
		"popl  %ecx           \n");

#endif
}

void CallThisCallFunctionRetByRef_impl(const void *obj, const asDWORD *args, int paramSize, asDWORD func, void *retPtr)
{
#if defined ASM_INTEL

	// Copy the data to the real stack. If we fail to do
	// this we may run into trouble in case of exceptions.
	__asm
	{
		// We must save registers that are used
	    push ecx			

		// Copy arguments from script  
		// stack to application stack
        mov  ecx, paramSize
		mov  eax, args
		add  eax, ecx
		cmp  ecx, 0
		je   endcopy
copyloop:
		sub  eax, 4
		push dword ptr [eax]
		sub  ecx, 4
		jne  copyloop
endcopy:

#ifdef THISCALL_PASS_OBJECT_POINTER_ON_THE_STACK
		// Push the object pointer on the stack
		push obj
#else
		// Move object pointer to ECX
		mov  ecx, obj
#endif

		// Push the return pointer
		push retPtr

		// Call function
		call [func]         

#ifndef THISCALL_CALLEE_POPS_ARGUMENTS
		// Pop arguments
		add  esp, paramSize
#ifdef THISCALL_PASS_OBJECT_POINTER_ON_THE_STACK
		// Pop object pointer
		add esp, 4
#endif
#endif

		// Restore registers
		pop  ecx

		// Return value in EAX or EAX:EDX
	}

#elif defined ASM_AT_N_T

	asm("pushl %ecx           \n"
		"movl  16(%ebp), %ecx \n" // paramSize
		"movl  12(%ebp), %eax \n" // args
		"addl  %ecx, %eax     \n" // push all arguments to the stack
		"cmp   $0, %ecx       \n"
		"je    endcopy3       \n"
		"copyloop3:           \n"
		"subl  $4, %eax       \n"
		"pushl (%eax)         \n"
		"subl  $4, %ecx       \n"
		"jne   copyloop3      \n"
		"endcopy3:            \n"
		"movl  8(%ebp), %ecx  \n" // move obj into ECX
		"pushl 8(%ebp)        \n" // push obj on the stack
		"pushl 24(%ebp)       \n" // push retPtr on the stack
		"call  *20(%ebp)      \n"
		"addl  16(%ebp), %esp \n" // pop arguments
		"addl  $4, %esp       \n" // pop the object pointer
		                          // the return pointer was popped by the callee
		"popl  %ecx           \n");

#endif
}

asDWORD GetReturnedFloat()
{
    asDWORD f;

#if defined ASM_INTEL

	// Get the float value from ST0
    __asm fstp dword ptr [f]

#elif defined ASM_AT_N_T

	asm("fstps %0 \n" : "=m" (f));

#endif

	return f;
}

asQWORD GetReturnedDouble()
{
    asQWORD d;

#if defined ASM_INTEL

	// Get the double value from ST0
    __asm fstp qword ptr [d]

#elif defined ASM_AT_N_T

	asm("fstpl %0 \n" : "=m" (d));

#endif

	return d;
}



