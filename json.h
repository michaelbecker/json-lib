#ifndef JSON_H__
#define JSON_H__


typedef enum _JSON_TYPE {

    TYPE_UNKNOWN,
    TYPE_OBJECT,
    TYPE_STRING,
    TYPE_BOOLEAN,
    TYPE_ARRAY,
    TYPE_NUMBER

} JSON_TYPE;


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
    ERROR_MAX_RECURSION,

}JSON_ERROR;


#define JSON_VALUE_SIGNATURE 0x4A56414C


// Forward Declaration
struct _JSON_MEMBER;


typedef struct _JSON_VALUE {

    int Signature;

    JSON_TYPE Type;

    union {
        char *String;
        double Number;
        int Boolean;
        struct _JSON_MEMBER *Object;
    };

} JSON_VALUE;


#define JSON_MEMBER_SIGNATURE 0x4A4D454D


typedef struct _JSON_MEMBER {

    int Signature;
    char *Name;
    JSON_VALUE *Value;
    struct _JSON_MEMBER *Next;

} JSON_MEMBER;


typedef JSON_MEMBER* JSON_OBJECT_HANDLE;


JSON_OBJECT_HANDLE JSON_Parse(char *string);
void JSON_Print(JSON_OBJECT_HANDLE object);
char* JSON_Stringify(JSON_OBJECT_HANDLE object);
void JSON_FreeObject(JSON_OBJECT_HANDLE object);

JSON_TYPE JSON_GetType(JSON_OBJECT_HANDLE object, char *path);
int JSON_GetBoolean(JSON_OBJECT_HANDLE object, char *name);
double JSON_GetNumber(JSON_OBJECT_HANDLE object, char *path);
char *JSON_GetString(JSON_OBJECT_HANDLE object, char *path);
JSON_OBJECT_HANDLE JSON_GetObject(JSON_OBJECT_HANDLE object, char *path);

JSON_OBJECT_HANDLE JSON_AllocObject(void);
JSON_ERROR JSON_AddBoolean(JSON_OBJECT_HANDLE object, char *name, int value);
JSON_ERROR JSON_AddString(JSON_OBJECT_HANDLE object, char *path, char *value);
JSON_ERROR JSON_AddNumber(JSON_OBJECT_HANDLE object, char *path, double value);

JSON_ERROR JSON_GetErrno(void);


void JSONDBG_Print(JSON_OBJECT_HANDLE object);


#endif
