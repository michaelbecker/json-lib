#ifndef JSON_H__
#define JSON_H__


/* Forward decl's */
struct _JSON_OBJECT;
struct _JSON_ARRAY;


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
    ERROR_TYPE_MISMATCH,
    ERROR_INVALID_VALUE_TYPE,
    ERROR_MAX_RECURSION,

}JSON_ERROR;


extern JSON_ERROR JSON_Errno;


typedef struct _JSON_ARRAY {
    int todo;
} JSON_ARRAY;


typedef struct _JSON_VALUE {

    JSON_TYPE Type;

    union {
        char *String;
        double Number;
        int Boolean;
        struct _JSON_OBJECT *Object;
        struct _JSON_ARRAY *Array;
    };

} JSON_VALUE;


typedef struct _JSON_MEMBER {

    char *Name;
    JSON_VALUE *Value;
    struct _JSON_MEMBER *Next;

} JSON_MEMBER;


typedef struct _JSON_OBJECT {

    JSON_MEMBER *Member;

} JSON_OBJECT;


JSON_OBJECT* JSON_Parse(char *string);
void JSON_Print(JSON_OBJECT *object);
char* JSON_Stringify(JSON_OBJECT *object);
void JSON_FreeObject(JSON_OBJECT *object);

JSON_TYPE JSON_GetType(JSON_OBJECT *object, char *path);
int JSON_GetBoolean(JSON_OBJECT *object, char *name);
double JSON_GetNumber(JSON_OBJECT *object, char *path);
char *JSON_GetString(JSON_OBJECT *object, char *path);
JSON_OBJECT *JSON_GetObject(JSON_OBJECT *object, char *path);

JSON_OBJECT *JSON_AllocObject(void);
JSON_ERROR JSON_AddBoolean(JSON_OBJECT *object, char *name, int value);
JSON_ERROR JSON_AddString(JSON_OBJECT *object, char *path, char *value);
JSON_ERROR JSON_AddNumber(JSON_OBJECT *object, char *path, double value);


#endif
