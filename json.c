#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <setjmp.h>
#include <errno.h>
#include "json.h"


#define ASSERT(_test_condition)                                 \
       if (!(_test_condition)){                                 \
           printf("ASSERT FAILED! \"%s\" %s:%d\n",              \
                        #_test_condition, __FILE__, __LINE__);  \
           abort();                                             \
       }


static jmp_buf parse_jmp_buffer;
static jmp_buf stringify_jmp_buffer;


JSON_ERROR JSON_Errno;


JSON_ERROR JSON_GetErrno(void)
{
    return JSON_Errno;
}


// Forward declaration
static struct _JSON_OBJECT* parseJsonObject(char **cursor);


static void skipBlanks(char **cursor)
{
    while (isblank(**cursor)) {
        (*cursor)++;
    }
}


static char* parseJsonString(char **cursor)
{
    char *start = NULL;
    char *end = NULL;
    char *new_string;

    // Find the first "
    start = strstr(*cursor, "\"");
    if (!start) {
        JSON_Errno = ERROR_INVALID_STRING;
        longjmp(parse_jmp_buffer, 1);
    }
    start++;

    // Find the last \" and null terminate the string
    end = strstr(start, "\"");
    if (!end) {
        JSON_Errno = ERROR_INVALID_STRING;
        longjmp(parse_jmp_buffer, 1);
    }

    *cursor = end + 1;

    new_string = strndup(start, end - start);
    if (!new_string){
        JSON_Errno = ERROR_ALLOC_FAILED;
        longjmp(parse_jmp_buffer, 1);
    }

    return new_string;
}


static int parseJsonBoolean(char **cursor)
{
    if (strncmp(*cursor, "true", 4) == 0) {
        *cursor += 4;
        return 1;
    }
    else if (strncmp(*cursor, "false", 5) == 0) {
        *cursor += 5;
        return 0;
    }
    else {
        JSON_Errno = ERROR_INVALID_BOOLEAN;
        longjmp(parse_jmp_buffer, 1);
    }
}


static double parseJsonNumber(char **cursor)
{
    char *end = NULL;
    double value = strtod(*cursor, &end);
    *cursor = end;
    return value;
}


static JSON_VALUE *allocJsonValue(void)
{
    //--------------------------
    JSON_VALUE *value;
    //--------------------------

    value = (JSON_VALUE *)malloc(sizeof(JSON_VALUE));
    if (!value) {
        JSON_Errno = ERROR_ALLOC_FAILED;
        return NULL;
    }

    memset(value, 0, sizeof(JSON_VALUE));
    value->Signature = JSON_VALUE_SIGNATURE;

    return value;
}


static JSON_MEMBER *allocJsonMember(void)
{
    //---------------------
    JSON_MEMBER *member;
    //---------------------

    member = (JSON_MEMBER*) malloc(sizeof(JSON_MEMBER));
    if (!member){
        JSON_Errno = ERROR_ALLOC_FAILED;
        return NULL;
    }

    memset(member, 0, sizeof(JSON_MEMBER));
    member->Signature = JSON_MEMBER_SIGNATURE;

    return member;
}


static JSON_OBJECT *allocJsonObject(void)
{
    //---------------------------
    JSON_OBJECT *object;
    //---------------------------

    object = (JSON_OBJECT*) malloc(sizeof(JSON_OBJECT));
    if(!object){
        JSON_Errno = ERROR_ALLOC_FAILED;
        return NULL;
    }

    memset(object, 0, sizeof(JSON_OBJECT));
    object->Signature = JSON_OBJECT_SIGNATURE;

    return object;
}


// Forward decl'
static JSON_VALUE* parseJsonValue(char **cursor);


static JSON_VALUE* parseJsonArray(char **cursor)
{
    //------------------------------------
    char *start = NULL;
    JSON_VALUE *value;
    JSON_VALUE *initial_value;
    //------------------------------------

    // Skip the leading [
    (*cursor)++;

    //  parseJsonValue() does all it's own error checking
    //  and handling.
    initial_value = parseJsonValue(cursor);
    value = initial_value;

    while (1) {
        // Find the next non-space character
        skipBlanks(cursor);

        // Decode it
        start = *cursor;
        if (*start == ',') {
            // Skip over the ,
            (*cursor)++;
            value->Next = parseJsonValue(cursor);
            value = value->Next;
        }
        else if (*start == ']') {
            // Skip over the trailing ]
            (*cursor)++;
            return initial_value;
        }
        else {
            JSON_Errno = ERROR_INVALID_ARRAY;
            longjmp(parse_jmp_buffer, 1);
        }
    }
}


static JSON_VALUE* parseJsonValue(char **cursor)
{
    //--------------------------
    char *start = NULL;
    JSON_VALUE *value;
    //--------------------------

    value = allocJsonValue();
    if (!value){
        JSON_Errno = ERROR_ALLOC_FAILED;
        longjmp(parse_jmp_buffer, 1);
    }

    // Find the next non-space character
    skipBlanks(cursor);

    start = *cursor;

    if (*start == '"') {
        value->Type = TYPE_STRING;
        value->String = parseJsonString(cursor);
    }
    else if (*start == 't') {
        value->Type = TYPE_BOOLEAN;
        value->Boolean = parseJsonBoolean(cursor);
    }
    else if (*start == 'f') {
        value->Type = TYPE_BOOLEAN;
        value->Boolean = parseJsonBoolean(cursor);
    }
    else if (isdigit(*start)) {
        value->Type = TYPE_NUMBER;
        value->Number = parseJsonNumber(cursor);
    }
    else if (*start == '[') {
        value->Type = TYPE_ARRAY;
        value->Array = parseJsonArray(cursor);
    }
    else if (*start == '{') {
        value->Type = TYPE_OBJECT;
        value->Object = parseJsonObject(cursor);
    }
    else {
        JSON_Errno = ERROR_INVALID_VALUE_TYPE;
        longjmp(parse_jmp_buffer, 1);
    }

    return value;
}


static JSON_MEMBER* parseJsonMember(char **cursor)
{
    //---------------------
    JSON_MEMBER *member;
    //---------------------

    member = allocJsonMember();
    if (!member){
        longjmp(parse_jmp_buffer, 1);
    }

    // Get the name
    member->Name = parseJsonString(cursor);

    // Find the :
    *cursor = strstr(*cursor, ":");
    if ((*cursor) == NULL){
        JSON_Errno = ERROR_INVALID_OBJECT;
        longjmp(parse_jmp_buffer, 1);
    }
    // Skip past the :
    (*cursor)++;

    // Get the value
    member->Value = parseJsonValue(cursor);
    member->Next = NULL;

    return member;
}


static JSON_OBJECT* parseJsonObject(char **cursor)
{
    //-----------------------------------------
    JSON_OBJECT *object;
    JSON_MEMBER *member;
    JSON_MEMBER *prior_member = NULL;
    int parsing_in_progress;
    //-----------------------------------------

    object = allocJsonObject();
    if(!object){
        longjmp(parse_jmp_buffer, 1);
    }

    *cursor = strstr(*cursor, "{");
    if ((*cursor) == NULL) {
        JSON_Errno = ERROR_INVALID_OBJECT;
        longjmp(parse_jmp_buffer, 1);
    }

    // Skip past the {
    (*cursor)++;

    do {
        member = parseJsonMember(cursor);

        if (!object->Member) {
            object->Member = member;
        }
        else {
            prior_member->Next = member;
        }
        prior_member = member;

        // Find the next non-space character
        skipBlanks(cursor);

        if (**cursor == ',') {
            parsing_in_progress = 1;
            (*cursor)++;
        }
        else if (**cursor == '}') {
            parsing_in_progress = 0;
            (*cursor)++;
        }
        else {
            JSON_Errno = ERROR_INVALID_OBJECT;
            longjmp(parse_jmp_buffer, 1);
        }

    } while (parsing_in_progress);

    return object;
}


JSON_OBJECT* JSON_Parse(char *string)
{
    //-----------------------
    JSON_OBJECT *object;
    int rc;
    //-----------------------

    JSON_Errno = SUCCESS;

    rc = setjmp(parse_jmp_buffer);
    if (rc == 0) {
        object = parseJsonObject(&string);
        return object;
    }
    else {
        return NULL;
    }
}


#define SPACES_PER_INDENTATION 4


static void printIndent(int indent_level)
{
    int i, j;
    for (i = 0; i < indent_level; i++)
        for (j = 0; j < SPACES_PER_INDENTATION; j++)
            printf(" ");

}


// Forward dec'l
static void printJsonObject(JSON_OBJECT *object, int *indent_level);
static void printJsonValue(JSON_VALUE *value, int *indent_level);


static void printJsonArray(JSON_VALUE *value, int *indent_level)
{
    ASSERT(value->Signature == JSON_VALUE_SIGNATURE);

    printf("[ ");

    while (value) {
        printJsonValue(value, indent_level);
        value = value->Next;
        if (value)
            printf(", ");
    }

    printf("] ");
}


static void printJsonValue(JSON_VALUE *value, int *indent_level)
{
    ASSERT(value->Signature == JSON_VALUE_SIGNATURE);

    switch (value->Type) {

        case TYPE_OBJECT:
            printJsonObject(value->Object, indent_level);
            break;

        case TYPE_ARRAY:
            printJsonArray(value->Array, indent_level);
            break;

        case TYPE_STRING:
            printf("\"%s\"", value->String);
            break;

        case TYPE_BOOLEAN:
            if (value->Boolean)
                printf("true");
            else
                printf("false");
            break;

        case TYPE_NUMBER:
            printf("%f", value->Number);
            break;

        default:
            break;
    }
}


static void printJsonMember(JSON_MEMBER *member, int *indent_level)
{
    ASSERT(member->Signature == JSON_MEMBER_SIGNATURE);

    printIndent(*indent_level);
    printf("\"%s\":", member->Name);
    printJsonValue(member->Value, indent_level);
}


static void printJsonObject(JSON_OBJECT *object, int *indent_level)
{
    //--------------------------
    JSON_MEMBER *member;
    //--------------------------

    ASSERT(object->Signature == JSON_OBJECT_SIGNATURE);

    printf("{\n");
    (*indent_level)++;

    member = object->Member;
    while (member) {
        printJsonMember(member, indent_level);
        member = member->Next;
        if (member) {
            printf(",\n");
        }
    }
    (*indent_level)--;
    printf("\n");
    printIndent(*indent_level);
    printf("}\n");
}


void JSON_Print(JSON_OBJECT *object)
{
    //--------------------------
    int indent_level = 0;
    //--------------------------

    JSON_Errno = SUCCESS;

    if (object->Signature != JSON_OBJECT_SIGNATURE)
        JSON_Errno = ERROR_INVALID_OBJECT;

    printJsonObject(object, &indent_level);
}


#define SMART_BUFFER_SIGNATURE 0x53627566


typedef struct _SMART_BUFFER {

    int Signature;
    char *buffer;
    int buffer_length;
    int length_used;

}SMART_BUFFER;


static void updateBuffer(SMART_BUFFER *sb)
{
    //----------------------
    char *new_buffer;
    int new_length = -1;
    int remaining_length;
    //----------------------

    if (sb->buffer_length <= 0) {
        new_length = 1024;
    }
    else {
        remaining_length = sb->buffer_length - sb->length_used;

        if (remaining_length < 512) {
            new_length = sb->buffer_length + 1024;
        }
    }

    if (new_length > 0) {
        new_buffer = (char*) realloc(sb->buffer, new_length);
        if (!new_buffer) {
            free(sb->buffer);
            JSON_Errno = ERROR_ALLOC_FAILED;
            longjmp(stringify_jmp_buffer, 1);
        }
        sb->buffer_length = new_length;
        sb->buffer = new_buffer;
    }
}


// Forward dec'l
static void stringifyJsonObject(JSON_OBJECT *object, SMART_BUFFER *sb);
static void stringifyJsonValue(JSON_VALUE *value, SMART_BUFFER *sb);


static void stringifyJsonArray(JSON_VALUE *value, SMART_BUFFER *sb)
{
    ASSERT(value->Signature == JSON_VALUE_SIGNATURE);
    ASSERT(sb->Signature == SMART_BUFFER_SIGNATURE);

    sb->length_used += sprintf( sb->buffer + sb->length_used, "[");

    while (value) {
        stringifyJsonValue(value, sb);
        value = value->Next;
        if (value)
            sb->length_used += sprintf( sb->buffer + sb->length_used, ",");
    }

    sb->length_used += sprintf( sb->buffer + sb->length_used, "]");
}


static void stringifyJsonValue(JSON_VALUE *value, SMART_BUFFER *sb)
{
    ASSERT(value->Signature == JSON_VALUE_SIGNATURE);
    ASSERT(sb->Signature == SMART_BUFFER_SIGNATURE);

    switch (value->Type) {

        case TYPE_OBJECT:
            stringifyJsonObject(value->Object, sb);
            break;

        case TYPE_ARRAY:
            stringifyJsonArray(value->Array, sb);
            break;

        case TYPE_STRING:
            sb->length_used += sprintf( sb->buffer + sb->length_used, "\"%s\"",
                                        value->String);
            break;

        case TYPE_BOOLEAN:
            if (value->Boolean)
                sb->length_used += sprintf( sb->buffer + sb->length_used, "true");
            else
                sb->length_used += sprintf( sb->buffer + sb->length_used, "false");
            break;

        case TYPE_NUMBER:
            sb->length_used += sprintf( sb->buffer + sb->length_used, "%f",
                                        value->Number);
            break;

        default:
            break;
    }

    updateBuffer(sb);
}


static void stringifyJsonMember(JSON_MEMBER *member, SMART_BUFFER *sb)
{
    ASSERT(member->Signature == JSON_MEMBER_SIGNATURE);
    ASSERT(sb->Signature == SMART_BUFFER_SIGNATURE);

    sb->length_used += sprintf( sb->buffer + sb->length_used, "\"%s\":",
                                member->Name);
    updateBuffer(sb);
    stringifyJsonValue(member->Value, sb);
}


static void stringifyJsonObject(JSON_OBJECT *object,
                                SMART_BUFFER *sb)
{
    //------------------------
    JSON_MEMBER *member;
    //------------------------

    ASSERT(object->Signature == JSON_OBJECT_SIGNATURE);
    ASSERT(sb->Signature == SMART_BUFFER_SIGNATURE);

    updateBuffer(sb);
    sb->length_used += sprintf(sb->buffer + sb->length_used, "{");

    member = object->Member;
    while (member) {
        stringifyJsonMember(member, sb);
        member = member->Next;
        if (member) {
            sb->length_used += sprintf(sb->buffer + sb->length_used, ",");
        }
    }
    sb->length_used += sprintf(sb->buffer + sb->length_used, "}");
}


char* JSON_Stringify(JSON_OBJECT *object)
{
    //-------------------------------
    SMART_BUFFER sb = {0};
    int rc;
    //-------------------------------

    JSON_Errno = SUCCESS;
    sb.Signature = SMART_BUFFER_SIGNATURE;

    if (object->Signature != JSON_OBJECT_SIGNATURE) {
        JSON_Errno = ERROR_INVALID_OBJECT;
        return NULL;
    }

    rc = setjmp(stringify_jmp_buffer);
    if (rc == 0) {
        stringifyJsonObject(object, &sb);
        return sb.buffer;
    }
    else {
        return NULL;
    }
}


// Forward decl
static void freeJsonObject(JSON_OBJECT *member);
static void freeJsonValue(JSON_VALUE *value);


static void freeJsonArray(JSON_VALUE *value)
{
    //-----------------------------
    JSON_VALUE *next_value;
    //-----------------------------

    ASSERT(value != NULL);
    ASSERT(value->Signature == JSON_VALUE_SIGNATURE);

    while (value) {
        next_value = value->Next;
        freeJsonValue(value);
        value = next_value;
    }
}


static void freeJsonValue(JSON_VALUE *value)
{
    ASSERT(value != NULL);
    ASSERT(value->Signature == JSON_VALUE_SIGNATURE);

    switch (value->Type) {

        case TYPE_OBJECT:
            freeJsonObject(value->Object);
            break;

        case TYPE_ARRAY:
            freeJsonArray(value->Array);
            break;

        case TYPE_STRING:
            free(value->String);
            break;

        case TYPE_BOOLEAN:
            break;

        case TYPE_NUMBER:
            break;

        default:
            break;
    }

    free(value);
}


static void freeJsonMember(JSON_MEMBER *member)
{
    ASSERT(member != NULL);
    ASSERT(member->Signature == JSON_MEMBER_SIGNATURE);

    free(member->Name);
    freeJsonValue(member->Value);
}


static void freeJsonObject(JSON_OBJECT *object)
{
    //------------------------
    JSON_MEMBER *member;
    JSON_MEMBER *prev_member;
    //------------------------

    ASSERT(object != NULL);
    ASSERT(object->Signature == JSON_OBJECT_SIGNATURE);

    member = object->Member;
    while (member) {
        freeJsonMember(member);
        prev_member = member;
        member = member->Next;
        free(prev_member);
    }
    free(object);
}


void JSON_FreeObject(JSON_OBJECT *object)
{
    JSON_Errno = SUCCESS;

    if (object->Signature == JSON_OBJECT_SIGNATURE) {
        freeJsonObject(object);
    }
    else {
        JSON_Errno = ERROR_INVALID_OBJECT;
    }
}


static JSON_MEMBER *findJsonMemberInObject(JSON_OBJECT *object, char *name)
{
    //------------------------
    JSON_MEMBER *member;
    //------------------------

    ASSERT(object != NULL);
    ASSERT(object->Signature == JSON_OBJECT_SIGNATURE);

    member = object->Member;
    while (member) {
        if (strcmp(member->Name, name) == 0){
            return member;
        }
        member = member->Next;
    }
    return NULL;
}


static void addJsonMemberToObject(JSON_OBJECT *object, JSON_MEMBER *new_member)
{
    //------------------------
    JSON_MEMBER *member;
    //------------------------

    ASSERT(object != NULL);
    ASSERT(object->Signature == JSON_OBJECT_SIGNATURE);
    ASSERT(new_member != NULL);
    ASSERT(new_member->Signature == JSON_MEMBER_SIGNATURE);

    if (!object->Member) {
        object->Member = new_member;
        return;
    }

    member = object->Member;
    while (1) {
        if (!member->Next) {
            member->Next = new_member;
            return;
        }
        member = member->Next;
    }
}


enum DOB_BRACKET_ORDER {
    DOB_LAST_ELEMENT,
    DOB_NEXT_OBJECT,
    DOB_NEXT_ARRAY,
};


typedef struct DotOrBracket_ {
    char *Dot;
    char *Bracket;
    enum DOB_BRACKET_ORDER Order;
    char *Path;
    int ArrayIndex;
}DotOrBracket;


static int parseArrayIndex(DotOrBracket *dob)
{
    //-----------------
    char *endptr;
    long index;
    //-----------------

    errno = 0;

    index = strtol(dob->Path, &endptr, 0);

    if (*endptr != ']') {
        JSON_Errno = ERROR_INVALID_ARRAY;
        return -1;
    }
    else {
        endptr++;
        dob->Path = endptr;
    }

    if (errno) {
        JSON_Errno = ERROR_INVALID_ARRAY;
        return -1;
    }

    return (int)index;
}


static char *getNextDotOrBracket(DotOrBracket *dob)
{
    //------------------
    char *name;
    //------------------

    dob->Bracket = strchr(dob->Path, '[');
    dob->Dot = strchr(dob->Path, '.');

    if (!dob->Dot && !dob->Bracket) {
        dob->Order = DOB_LAST_ELEMENT;
        name = dob->Path;
    }
    else if (!dob->Dot) {
        dob->Order = DOB_NEXT_ARRAY;
        name = dob->Path;
        *(dob->Bracket) = 0;
        dob->Path = dob->Bracket + 1;
        dob->ArrayIndex = parseArrayIndex(dob);
    }
    else if (!dob->Bracket) {
        dob->Order = DOB_NEXT_OBJECT;
        name = dob->Path;
        *(dob->Dot) = 0;
        dob->Path = dob->Dot + 1;
    }
    else if (dob->Bracket > dob->Dot) {
        dob->Order = DOB_NEXT_OBJECT;
        name = dob->Path;
        *(dob->Dot) = 0;
        dob->Path = dob->Dot + 1;
    }
    else {
        dob->Order = DOB_NEXT_ARRAY;
        name = dob->Path;
        *(dob->Bracket) = 0;
        dob->Path = dob->Bracket + 1;
        dob->ArrayIndex = parseArrayIndex(dob);
    }

    return name;
}


JSON_VALUE *findJsonValueInArray(JSON_VALUE *value, int index)
{
    //------------------------
    int i = 0;
    //------------------------

    while (i < index) {
        value = value->Next;
        if (!value)
            return NULL;
        i++;
    }

    return value;
}


static JSON_VALUE *findJsonValue(char *path, JSON_OBJECT *object)
{
    //------------------------
    JSON_MEMBER *member;
    JSON_VALUE *value;
    char *name;
    char *path_copy;
    DotOrBracket dob;
    //------------------------

    path_copy = strdup(path);
    dob.Path = path_copy;

    while ((name = getNextDotOrBracket(&dob)) != NULL) {

        if (dob.Order == DOB_LAST_ELEMENT) {
            member = findJsonMemberInObject(object, name);
            if (!member) {
                goto FIND_FAILED;
            }
            else if (member->Value->Type == TYPE_OBJECT) {
                goto FIND_FAILED;
            }
            else {
                value = member->Value;
                break;
            }
        }
        else if (dob.Order == DOB_NEXT_OBJECT) {
            member = findJsonMemberInObject(object, name);
            if (!member) {
                goto FIND_FAILED;
            }
            else if (member->Value->Type != TYPE_OBJECT) {
                goto FIND_FAILED;
            }
            else {
                object = member->Value->Object;
            }
        }
        else {
            // dob.Order == DOB_NEXT_ARRAY
            if (dob.ArrayIndex < 0) {
                goto FIND_FAILED;
            }

            member = findJsonMemberInObject(object, name);
            if (!member)
                goto FIND_FAILED;

            if (member->Value->Type != TYPE_ARRAY)
                goto FIND_FAILED;
            else
                value = member->Value->Array;

            value = findJsonValueInArray(value, dob.ArrayIndex);
            if (!value)
                goto FIND_FAILED;

            if (value->Type == TYPE_OBJECT)
                object = value->Object;
            else if (value->Type == TYPE_ARRAY)
                ;
            else
                break;
        }
    }

    // If we make it here, we found the value.
    free(path_copy);
    return value;

    // We didn't find the value, so cleanup.
FIND_FAILED:
    free(path_copy);
    return NULL;
}

JSON_TYPE JSON_GetType(JSON_OBJECT *object, char *path)
{
    //------------------------
    JSON_VALUE *value;
    //------------------------

    JSON_Errno = SUCCESS;

    value = findJsonValue(path, object);

    if (value != NULL)
        return value->Type;
    else
        return TYPE_UNKNOWN;
}


int JSON_GetBoolean(JSON_OBJECT *object, char *path)
{
    //------------------------
    JSON_VALUE *value;
    //------------------------

    JSON_Errno = SUCCESS;

    value = findJsonValue(path, object);

    if ((value != NULL) && (value->Type == TYPE_BOOLEAN)) {
        return value->Boolean;
    }
    else {
        JSON_Errno = ERROR_INVALID_BOOLEAN;
        return -ERROR_INVALID_BOOLEAN;
    }
}


double JSON_GetNumber(JSON_OBJECT *object, char *path)
{
    //------------------------
    JSON_VALUE *value;
    //------------------------

    JSON_Errno = SUCCESS;

    value = findJsonValue(path, object);

    if ((value != NULL) && (value->Type == TYPE_NUMBER)) {
        return value->Number;
    }
    else {
        JSON_Errno = ERROR_INVALID_NUMBER;
        return 0;
    }
}


char *JSON_GetString(JSON_OBJECT *object, char *path)
{
    //------------------------
    JSON_VALUE *value;
    //------------------------

    JSON_Errno = SUCCESS;

    value = findJsonValue(path, object);

    if ((value != NULL) && (value->Type == TYPE_STRING)) {
        return value->String;
    }
    else {
        JSON_Errno = ERROR_INVALID_STRING;
        return NULL;
    }
}


JSON_OBJECT *JSON_GetObject(JSON_OBJECT *object, char *path)
{
    //------------------------
    JSON_VALUE *value;
    //------------------------

    JSON_Errno = SUCCESS;

    value = findJsonValue(path, object);

    if ((value != NULL) && (value->Type == TYPE_OBJECT)){
        return value->Object;
    }
    else {
        JSON_Errno = ERROR_INVALID_OBJECT;
        return NULL;
    }
}


JSON_OBJECT *JSON_AllocObject(void)
{
    //---------------------------
    JSON_OBJECT *object;
    //---------------------------

    JSON_Errno = SUCCESS;

    object = allocJsonObject();

    return object;
}


static JSON_MEMBER *allocJsonMemberAndValue(char *name)
{
    //------------------------
    JSON_MEMBER *member;
    //------------------------

    member = allocJsonMember();
    if (!member) {
        return NULL;
    }
    member->Name = strdup(name);

    member->Value = allocJsonValue();
    if (!member->Value) {
        free(member);
        return NULL;
    }

    return member;
}


static JSON_ERROR jsonAddValue(JSON_OBJECT *object, char *path, JSON_VALUE value)
{
    //-----------------------------
    JSON_MEMBER *member;
    char *name1;
    char *name2;
    char *path_copy;
    int member_should_be_object;
    JSON_ERROR rc = SUCCESS;
    //-----------------------------

    path_copy = strdup(path);

    name1 = strtok(path_copy, ".");

    do {
        name2 = strtok(NULL, ".");
        if (name2)
            member_should_be_object = 1;
        else
            member_should_be_object = 0;

        member = findJsonMemberInObject(object, name1);
        if (!member) {
            member = allocJsonMemberAndValue(name1);
            if (!member) {
                rc = ERROR_ALLOC_FAILED;
                break;
            }
            addJsonMemberToObject(object, member);
            if (member_should_be_object) {
                member->Value->Type = TYPE_OBJECT;
                member->Value->Object = JSON_AllocObject();
                name1 = name2;
                object = member->Value->Object;
            }
            else {
                memcpy(member->Value, &value, sizeof(JSON_VALUE));
                break;
            }
        }
        else if (!member_should_be_object &&
                  (member->Value->Type == value.Type)) {
            memcpy(member->Value, &value, sizeof(JSON_VALUE));
            break;
        }
        else if (member_should_be_object &&
                 (member->Value->Type == TYPE_OBJECT)) {
            name1 = name2;
            object = member->Value->Object;
        }
        else {
            rc = ERROR_TYPE_MISMATCH;
            break;
        }

    } while (1);

    free(path_copy);

    return rc;
}


JSON_ERROR JSON_AddBoolean(JSON_OBJECT *object, char *path, int value)
{
    //-----------------------------
    JSON_VALUE json_value;
    JSON_ERROR rc = SUCCESS;
    //-----------------------------

    JSON_Errno = SUCCESS;

    json_value.Type = TYPE_BOOLEAN;
    json_value.Boolean = value ? 1 : 0;

    rc = jsonAddValue(object, path, json_value);

    return rc;
}


JSON_ERROR JSON_AddString(JSON_OBJECT *object, char *path, char *value)
{
    //-----------------------------
    JSON_VALUE json_value;
    JSON_ERROR rc = SUCCESS;
    //-----------------------------

    JSON_Errno = SUCCESS;

    json_value.Type = TYPE_STRING;
    json_value.String = strdup(value);

    rc = jsonAddValue(object, path, json_value);

    return rc;
}


JSON_ERROR JSON_AddNumber(JSON_OBJECT *object, char *path, double value)
{
    //-----------------------------
    JSON_VALUE json_value;
    JSON_ERROR rc = SUCCESS;
    //-----------------------------

    JSON_Errno = SUCCESS;

    json_value.Type = TYPE_NUMBER;
    json_value.Number = value;

    rc = jsonAddValue(object, path, json_value);

    return rc;
}


