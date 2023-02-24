//---------------------------------------------------------------------------
//  json.h
//
//  This is the standard usage interface to this JSON library.
//
//  (c)2023, Michael Becker <michael.f.becker@gmail.com>
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful, but
//  WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
//  General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program. If not, see <https://www.gnu.org/licenses/>.
//
//---------------------------------------------------------------------------
#ifndef JSON_H__
#define JSON_H__


//---------------------------------------------------------------------------
//
//  Define to include the JSONDBG_Print() function.
//  Undefine this to make the code smaller.
//---------------------------------------------------------------------------
#define JSON_DBG_PRINT


//---------------------------------------------------------------------------
//
//  Define to include the JSON_Print() function.
//  Undefine this to make the code smaller.
//---------------------------------------------------------------------------
#define JSON_PRINT


//---------------------------------------------------------------------------
//
//  Errors returned by this library.
//
//---------------------------------------------------------------------------
typedef enum _JSON_ERROR {

    SUCCESS,
    ERROR_ALLOC_FAILED,
    ERROR_INVALID_STRING,
    ERROR_INVALID_BOOLEAN,
    ERROR_INVALID_NUMBER,
    ERROR_INVALID_OBJECT,
    ERROR_INVALID_ARRAY,
    ERROR_TYPE_MISMATCH,
    ERROR_INVALID_VALUE_TYPE,
    ERROR_INVALID_JSON_PATH

}JSON_ERROR;


//---------------------------------------------------------------------------
//
//  Standard types supported by JSON.
//
//---------------------------------------------------------------------------
typedef enum _JSON_TYPE {

    TYPE_UNKNOWN,
    TYPE_OBJECT,
    TYPE_STRING,
    TYPE_BOOLEAN,
    TYPE_ARRAY,
    TYPE_NUMBER

} JSON_TYPE;


//---------------------------------------------------------------------------
//
//  OBJECT_HANDLE is an opaque pointer to a fully parsed JSON object
//  usable by this library.
//
//---------------------------------------------------------------------------
typedef void* JSON_OBJECT_HANDLE;


//---------------------------------------------------------------------------
//
//  JSON_Parse()
//
//  Takes a complete JSON object as an ASCII string and returns
//  a handle to a corresponding JSON object in memory.
//
//---------------------------------------------------------------------------
JSON_OBJECT_HANDLE JSON_Parse(char *string);


#ifdef JSON_PRINT
//---------------------------------------------------------------------------
//
//  JSON_Print()
//
//  This function prints the JSON object formatted to stdout.
//
//---------------------------------------------------------------------------
void JSON_Print(JSON_OBJECT_HANDLE object);

#endif


//---------------------------------------------------------------------------
//
//  JSON_Stringify()
//
//  This function returns a pointer to an ASCII string representation
//  of the object. You are responsible for free'ing the memory once
//  you are done with it.
//
//---------------------------------------------------------------------------
char* JSON_Stringify(JSON_OBJECT_HANDLE object);


//---------------------------------------------------------------------------
//
//  JSON_FreeObject()
//
//  This function frees all memory allocated from this library
//  related to the in-memory storage of a JSON object.
//
//---------------------------------------------------------------------------
void JSON_FreeObject(JSON_OBJECT_HANDLE object);


//---------------------------------------------------------------------------
//
//  JSON_GetType()
//
//  This function frees all memory allocated from this library
//  related to the in-memory storage of a JSON object.
//
//---------------------------------------------------------------------------
JSON_TYPE JSON_GetType(JSON_OBJECT_HANDLE object, char *path);


//---------------------------------------------------------------------------
//
//  JSON_GetBoolean()
//
//  This function returns the value of a boolean member of a JSON object.
//
//---------------------------------------------------------------------------
int JSON_GetBoolean(JSON_OBJECT_HANDLE object, char *name);


//---------------------------------------------------------------------------
//
//  JSON_GetNumber()
//
//  This function returns the value of a numeric member of a JSON object.
//
//---------------------------------------------------------------------------
double JSON_GetNumber(JSON_OBJECT_HANDLE object, char *path);


//---------------------------------------------------------------------------
//
//  JSON_GetString()
//
//  This function returns the value of a string member of a JSON object.
//  This is the actual string from the object, do not edit it.
//
//---------------------------------------------------------------------------
char *JSON_GetString(JSON_OBJECT_HANDLE object, char *path);


//---------------------------------------------------------------------------
//
//  JSON_GetObject()
//
//  This function returns the handle of an object.
//
//---------------------------------------------------------------------------
JSON_OBJECT_HANDLE JSON_GetObject(JSON_OBJECT_HANDLE object, char *path);


//---------------------------------------------------------------------------
//
//  JSON_AllocObject()
//
//  This function returns the handle of a newly created, empty object.
//
//---------------------------------------------------------------------------
JSON_OBJECT_HANDLE JSON_AllocObject(void);


//---------------------------------------------------------------------------
//
//  JSON_AddBoolean()
//
//  This function adds a boolean value to an object.
//
//---------------------------------------------------------------------------
JSON_ERROR JSON_AddBoolean(JSON_OBJECT_HANDLE object, char *name, int value);


//---------------------------------------------------------------------------
//
//  JSON_AddString()
//
//  This function adds a string value to an object. A copy of the string
//  passed in is made.
//
//---------------------------------------------------------------------------
JSON_ERROR JSON_AddString(JSON_OBJECT_HANDLE object, char *path, char *value);


//---------------------------------------------------------------------------
//
//  JSON_AddNumber()
//
//  This function adds a numeric value to an object.
//
//---------------------------------------------------------------------------
JSON_ERROR JSON_AddNumber(JSON_OBJECT_HANDLE object, char *path, double value);


//---------------------------------------------------------------------------
//
//  JSON_GetErrno()
//
//  This function returns the JSON specific errno from the last operation
//  performed.
//
//---------------------------------------------------------------------------
JSON_ERROR JSON_GetErrno(void);


#ifdef JSON_DBG_PRINT
//---------------------------------------------------------------------------
//
//  JSONDBG_Print()
//
//  Prints a detailed debug output of a JSON object.
//
//---------------------------------------------------------------------------
void JSONDBG_Print(JSON_OBJECT_HANDLE object);

#endif


#endif
