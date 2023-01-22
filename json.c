#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <setjmp.h>
#include "json.h"


#define ASSERT(_test_condition)                                 \
       if (!(_test_condition)){                                 \
           printf("ASSERT FAILED! \"%s\" %s:%d\n",          	\
                        #_test_condition, __FILE__, __LINE__);  \
           abort();                                             \
       }


static jmp_buf parse_jmp_buffer;
static jmp_buf stringify_jmp_buffer;


JSON_ERROR JSON_Errno;


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
    return value;
}


// Forward decl'
static JSON_VALUE* parseJsonValue(char **cursor);;


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

    member = (JSON_MEMBER*) malloc(sizeof(JSON_MEMBER));
    if (!member){
        JSON_Errno = ERROR_ALLOC_FAILED;
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

    object = (JSON_OBJECT*) malloc(sizeof(JSON_OBJECT));
    if(!object){
        JSON_Errno = ERROR_ALLOC_FAILED;
        longjmp(parse_jmp_buffer, 1);
    }
    memset(object, 0, sizeof(JSON_OBJECT));

    *cursor = strstr(*cursor, "{");
    ASSERT((*cursor) != NULL);
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


static void printJsonValue(JSON_VALUE *value, int *indent_level)
{
    switch (value->Type) {

        case TYPE_OBJECT:
            printJsonObject(value->Object, indent_level);
            break;

        case TYPE_ARRAY:
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
    printIndent(*indent_level);
    printf("\"%s\":", member->Name);
    printJsonValue(member->Value, indent_level);
}


static void printJsonObject(JSON_OBJECT *object, int *indent_level)
{
    //--------------------------
    JSON_MEMBER *member;
    //--------------------------

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

    printJsonObject(object, &indent_level);
}


typedef struct _SMART_BUFFER {

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
            JSON_Errno = ERROR_ALLOC_FAILED;
            longjmp(stringify_jmp_buffer, 1);
        }
        sb->buffer_length = new_length;
        sb->buffer = new_buffer;
    }
}


// Forward dec'l
static void stringifyJsonObject(JSON_OBJECT *object, SMART_BUFFER *sb);


static void stringifyJsonValue(JSON_VALUE *value, SMART_BUFFER *sb)
{
    switch (value->Type) {

        case TYPE_OBJECT:
            stringifyJsonObject(value->Object, sb);
            break;

        case TYPE_ARRAY:
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


static void freeJsonValue(JSON_VALUE *value)
{
    switch (value->Type) {

        case TYPE_OBJECT:
            freeJsonObject(value->Object);
            break;

        case TYPE_ARRAY:
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
    free(member->Name);
    freeJsonValue(member->Value);
}


static void freeJsonObject(JSON_OBJECT *object)
{
    //------------------------
    JSON_MEMBER *member;
    JSON_MEMBER *prev_member;
    //------------------------

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
    freeJsonObject(object);
}


static JSON_MEMBER *findJsonMemberInObject(JSON_OBJECT *object, char *name)
{
    //------------------------
    JSON_MEMBER *member;
    //------------------------

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


static JSON_VALUE *findJsonValue(char *path, JSON_OBJECT *object)
{
    //------------------------
    JSON_MEMBER *member;
    char *name;
    char *path_copy;
    //------------------------

    path_copy = strdup(path);

    name = strtok(path_copy, ".");
    member = findJsonMemberInObject(object, name);
    if (!member)
        goto FIND_FAILED;

    while ((name = strtok(NULL, ".")) != NULL) {

        if (member->Value->Type != TYPE_OBJECT)
            goto FIND_FAILED;

        member = findJsonMemberInObject(member->Value->Object, name);
        if (!member)
            goto FIND_FAILED;
    }

    // If we make it here, we found the value.
    free(path_copy);
    return member->Value;

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

    object = (JSON_OBJECT*) malloc(sizeof(JSON_OBJECT));
    if (!object) {
        JSON_Errno = ERROR_ALLOC_FAILED;
        return NULL;
    }
    memset(object, 0, sizeof(JSON_OBJECT));

    return object;
}


static JSON_MEMBER *allocJsonMemberAndValue(char *name)
{
    //------------------------
    JSON_MEMBER *member;
    //------------------------

    member = (JSON_MEMBER*) malloc(sizeof(JSON_MEMBER));
    if (!member) {
        JSON_Errno = ERROR_ALLOC_FAILED;
        return NULL;
    }
    memset(member, 0, sizeof(JSON_MEMBER));
    member->Name = strdup(name);

    member->Value = allocJsonValue();
    if (!member->Value) {
        JSON_Errno = ERROR_ALLOC_FAILED;
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


int main(void) {
    char test1[] =
            "{ \"x\" : true, \"y\": 123.456,\"z\": false, \"w\"\t:\"Hello World\"}";
    char test2[] =
            "{ \"x\" : true, \"y\": {\"y1\": \"Hello\", \"y2\" : \"World\", \"y3\" : true  }  }";
    char test3[] =
            "{ \"x1\" : {\"x2\" : 111, \"y2\" : {\"x3\":100, \"y3\":123, \"z3\":23}},"
                    " \"y1\": {\"x2\": 1234, \"y2\" : 765  }  }";

    JSON_OBJECT *object;
    char *buffer;
    int b;

    printf("JSON TEST\n");

    object = JSON_Parse(test1);
    JSON_Print(object);
    buffer = JSON_Stringify(object);
    printf("%s\n\n", buffer);
    b = JSON_GetBoolean(object, "x");
    printf("JSON_GetBoolean x = %d\n", b);
    b = JSON_GetBoolean(object, "z");
    printf("JSON_GetBoolean x = %d\n", b);
    b = JSON_GetBoolean(object, "unk");
    printf("JSON_GetBoolean unk = %d\n", b);
    JSON_FreeObject(object);

    object = JSON_Parse(test2);
    JSON_Print(object);
    buffer = JSON_Stringify(object);
    printf("%s\n\n", buffer);
    b = JSON_GetBoolean(object, "y.y3");
    printf("JSON_GetBoolean y.y3 = %d\n", b);
    JSON_FreeObject(object);

    object = JSON_Parse(test3);
    JSON_Print(object);
    buffer = JSON_Stringify(object);
    printf("%s\n\n", buffer);
    JSON_FreeObject(object);

    object = JSON_AllocObject();
    JSON_AddBoolean(object, "testBool2", 0);
    JSON_AddBoolean(object, "testBool3", 0);
    JSON_AddBoolean(object, "testBool1.sub1", 1);
    JSON_AddBoolean(object, "testBool1.sub3", 1);
    JSON_AddBoolean(object, "testBool1.sub2.x", 1);
    JSON_AddString(object, "testBool1.sub4.ProductName", "C Programming");
    JSON_AddNumber(object, "testBool1.sub4.ProductCost", 29.95);
    JSON_Print(object);
    JSON_FreeObject(object);

    return 0;
}

