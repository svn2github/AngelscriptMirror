#ifndef SCRIPTGRID_H
#define SCRIPTGRID_H

#ifndef ANGELSCRIPT_H 
// Avoid having to inform include path if header is already include before
#include <angelscript.h>
#endif

BEGIN_AS_NAMESPACE

struct SGridBuffer;

class CScriptGrid
{
public:
	// Set the memory functions that should be used by all CScriptGrids
	static void SetMemoryFunctions(asALLOCFUNC_t allocFunc, asFREEFUNC_t freeFunc);

	// Factory functions
	static CScriptGrid *Create(asIObjectType *ot, asUINT width, asUINT height);
	static CScriptGrid *Create(asIObjectType *ot, asUINT width, asUINT height, void *defaultValue);
	static CScriptGrid *Create(asIObjectType *ot, void *listBuffer);

	// Memory management
	void AddRef() const;
	void Release() const;

	// Type information
	asIObjectType *GetGridObjectType() const;
	int            GetGridTypeId() const;
	int            GetElementTypeId() const;

	asUINT GetWidth() const;
	asUINT GetHeight() const;

	// Get a pointer to an element. Returns 0 if out of bounds
	void       *At(asUINT x, asUINT y);
	const void *At(asUINT x, asUINT y) const;

	// Set value of an element
	// Remember, if the grid holds handles the value parameter should be the 
	// address of the handle. The refCount of the object will also be incremented
	void  SetValue(asUINT x, asUINT y, void *value);

	// GC methods
	int  GetRefCount();
	void SetFlag();
	bool GetFlag();
	void EnumReferences(asIScriptEngine *engine);
	void ReleaseAllHandles(asIScriptEngine *engine);

protected:
	mutable int       refCount;
	mutable bool      gcFlag;
	asIObjectType    *objType;
	SGridBuffer      *buffer;
	int               elementSize;
	int               subTypeId;

	// Constructors
	CScriptGrid(asIObjectType *ot, void *initBuf); // Called from script when initialized with list
	CScriptGrid(asUINT w, asUINT h, asIObjectType *ot);
	CScriptGrid(asUINT w, asUINT h, void *defVal, asIObjectType *ot);
	virtual ~CScriptGrid();

	bool  CheckMaxSize(asUINT x, asUINT y);
	void  CreateBuffer(SGridBuffer **buf, asUINT w, asUINT h);
	void  DeleteBuffer(SGridBuffer *buf);
	void  Construct(SGridBuffer *buf);
	void  Destruct(SGridBuffer *buf);
};

void RegisterScriptGrid(asIScriptEngine *engine);

END_AS_NAMESPACE

#endif
