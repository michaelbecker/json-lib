//---------------------------------------------------------------------------
//  json.c
//
//  This is the implementation of the JSON library.
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


#define JSON_VALUE_SIGNATURE 0x6C61764A


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


#define JSON_MEMBER_SIGNATURE 0x6D656D4A


typedef struct _JSON_MEMBER {

    int Signature;
    char *Name;
    JSON_VALUE *Value;
    struct _JSON_MEMBER *Next;

} JSON_MEMBER;



static jmp_buf parse_jmp_buffer;
static jmp_buf stringify_jmp_buffer;


JSON_ERROR JSON_Errno;


JSON_ERROR JSON_GetErrno(void)
{
    return JSON_Errno;
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


// Forward declaration
static JSON_MEMBER* parseJsonObject(char **cursor);


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


// Forward declarations
static JSON_VALUE* parseJsonValue(char **cursor);
static JSON_MEMBER* parseJsonArrayMember(char **cursor);


static JSON_MEMBER* parseJsonArray(char **cursor)
{
    //------------------------------------
    JSON_MEMBER *member;
    JSON_MEMBER *new_member;
    JSON_MEMBER *root_member = NULL;
    int parsing_in_progress;
    //------------------------------------

    *cursor = strstr(*cursor, "[");
    if ((*cursor) == NULL) {
        JSON_Errno = ERROR_INVALID_ARRAY;
        longjmp(parse_jmp_buffer, 1);
    }

    // Skip past the [
    (*cursor)++;

    do {
        new_member = parseJsonArrayMember(cursor);

        if (!root_member) {
            root_member = new_member;
            member = new_member;
        }
        else {
            member->Next = new_member;
            member = new_member;
        }

        // Find the next non-space character
        skipBlanks(cursor);

        if (**cursor == ',') {
            parsing_in_progress = 1;
            (*cursor)++;
        }
        else if (**cursor == ']') {
            parsing_in_progress = 0;
            (*cursor)++;
        }
        else {
            JSON_Errno = ERROR_INVALID_OBJECT;
            longjmp(parse_jmp_buffer, 1);
        }

    } while (parsing_in_progress);

    return root_member;
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
        value->Object = parseJsonArray(cursor);
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


static JSON_MEMBER* parseJsonObjectMember(char **cursor)
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

    return member;
}


static JSON_MEMBER* parseJsonArrayMember(char **cursor)
{
    //---------------------
    JSON_MEMBER *member;
    //---------------------

    member = allocJsonMember();
    if (!member){
        longjmp(parse_jmp_buffer, 1);
    }

    // Get the value
    member->Value = parseJsonValue(cursor);

    return member;
}


static JSON_MEMBER* parseJsonObject(char **cursor)
{
    //-----------------------------------------
    JSON_MEMBER *member;
    JSON_MEMBER *new_member;
    JSON_MEMBER *root_member = NULL;
    int parsing_in_progress;
    //-----------------------------------------

    *cursor = strstr(*cursor, "{");
    if ((*cursor) == NULL) {
        JSON_Errno = ERROR_INVALID_OBJECT;
        longjmp(parse_jmp_buffer, 1);
    }

    // Skip past the {
    (*cursor)++;

    do {
        new_member = parseJsonObjectMember(cursor);

        if (!root_member) {
            root_member = new_member;
            member = new_member;
        }
        else {
            member->Next = new_member;
            member = new_member;
        }

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

    return root_member;
}


JSON_OBJECT_HANDLE JSON_Parse(char *string)
{
    //-----------------------
    JSON_MEMBER *object;
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
static void printJsonObject(JSON_MEMBER *object, int *indent_level);
static void printJsonValue(JSON_VALUE *value, int *indent_level);


static void printJsonArray(JSON_MEMBER *member, int *indent_level)
{
    ASSERT(member->Signature == JSON_MEMBER_SIGNATURE);

    printf("[\n");
    (*indent_level)++;

    while (member) {

        printIndent(*indent_level);
        printJsonValue(member->Value, indent_level);
        member = member->Next;
        if (member)
            printf(",\n");
        else
            printf("\n");
    }

    (*indent_level)--;
    printIndent(*indent_level);
    printf("]");
}


static void printJsonValue(JSON_VALUE *value, int *indent_level)
{
    ASSERT(value->Signature == JSON_VALUE_SIGNATURE);

    switch (value->Type) {

        case TYPE_OBJECT:
            printJsonObject(value->Object, indent_level);
            break;

        case TYPE_ARRAY:
            printJsonArray(value->Object, indent_level);
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


static void printJsonObject(JSON_MEMBER *member, int *indent_level)
{
    ASSERT(member->Signature == JSON_MEMBER_SIGNATURE);

    printf("{\n");
    (*indent_level)++;

    while (member) {
        printJsonMember(member, indent_level);
        member = member->Next;
        if (member)
            printf(",\n");
        else
            printf("\n");
    }
    (*indent_level)--;
    printIndent(*indent_level);
    printf("}");
}


void JSON_Print(JSON_OBJECT_HANDLE object)
{
    //--------------------------
    int indent_level = 0;
    JSON_MEMBER *member;
    //--------------------------

    JSON_Errno = SUCCESS;
    member = object;

    if (!object || member->Signature != JSON_MEMBER_SIGNATURE)
        JSON_Errno = ERROR_INVALID_OBJECT;

    printJsonObject(member, &indent_level);
    printf("\n");
}


static void dbgPrintType(JSON_TYPE type)
{
    printf("Value Type: ");
    switch(type) {
        case TYPE_UNKNOWN: printf("TYPE_UNKNOWN\n"); break;
        case TYPE_OBJECT: printf("TYPE_OBJECT\n"); break;
        case TYPE_STRING: printf("TYPE_STRING\n"); break;
        case TYPE_BOOLEAN: printf("TYPE_BOOLEAN\n"); break;
        case TYPE_ARRAY: printf("TYPE_ARRAY\n"); break;
        case TYPE_NUMBER: printf("TYPE_NUMBER\n"); break;
        default: printf("Invalid!!!\n"); break;
    }
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
static void stringifyJsonObject(JSON_MEMBER *member, SMART_BUFFER *sb);
static void stringifyJsonValue(JSON_VALUE *value, SMART_BUFFER *sb);


static void stringifyJsonArray(JSON_MEMBER *member, SMART_BUFFER *sb)
{
    ASSERT(member->Signature == JSON_MEMBER_SIGNATURE);

    sb->length_used += sprintf( sb->buffer + sb->length_used, "[");

    while (member) {
        stringifyJsonValue(member->Value, sb);
        member = member->Next;
        if (member)
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
            stringifyJsonArray(value->Object, sb);
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


static void stringifyJsonObject(JSON_MEMBER *member, SMART_BUFFER *sb)
{
    ASSERT(member->Signature == JSON_MEMBER_SIGNATURE);

    updateBuffer(sb);
    sb->length_used += sprintf(sb->buffer + sb->length_used, "{");

    while (member) {
        stringifyJsonMember(member, sb);
        member = member->Next;
        if (member) {
            sb->length_used += sprintf(sb->buffer + sb->length_used, ",");
        }
    }
    sb->length_used += sprintf(sb->buffer + sb->length_used, "}");
}


char* JSON_Stringify(JSON_OBJECT_HANDLE object)
{
    //-------------------------------
    SMART_BUFFER sb = {0};
    int rc;
    JSON_MEMBER *member;
    //-------------------------------

    JSON_Errno = SUCCESS;
    member = object;

    sb.Signature = SMART_BUFFER_SIGNATURE;

    if (!object || member->Signature != JSON_MEMBER_SIGNATURE) {
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
static void freeJsonObject(JSON_MEMBER *member);
static void freeJsonValue(JSON_VALUE *value);
static void freeJsonMember(JSON_MEMBER *member);


static void freeJsonArray(JSON_MEMBER *member)
{
    //-----------------------------
    JSON_MEMBER *next_member;
    //-----------------------------

    ASSERT(member != NULL);
    ASSERT(member->Signature == JSON_MEMBER_SIGNATURE);

    while (member) {
        next_member = member->Next;
        freeJsonMember(member);
        member = next_member;
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
            freeJsonArray(value->Object);
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

    if (member->Name) {
        free(member->Name);
        member->Name = NULL;
    }

    freeJsonValue(member->Value);
}


static void freeJsonObject(JSON_MEMBER *member)
{
    //------------------------
    JSON_MEMBER *next_member;
    //------------------------

    ASSERT(member != NULL);
    ASSERT(member->Signature == JSON_MEMBER_SIGNATURE);

    while (member) {
        next_member = member->Next;
        freeJsonMember(member);
        member = next_member;
    }
}


void JSON_FreeObject(JSON_OBJECT_HANDLE object)
{
    //-------------------------------
    JSON_MEMBER *member;
    //-------------------------------

    JSON_Errno = SUCCESS;
    member = object;

    if (!object || member->Signature != JSON_MEMBER_SIGNATURE) {
        JSON_Errno = ERROR_INVALID_OBJECT;
        return;
    }

    freeJsonObject(object);
}


static JSON_MEMBER *findJsonMemberInObject(JSON_MEMBER *member, char *name)
{
    ASSERT(member != NULL);
    ASSERT(member->Signature == JSON_MEMBER_SIGNATURE);

    while (member) {
        if (strcmp(member->Name, name) == 0){
            return member;
        }
        member = member->Next;
    }
    return NULL;
}


static void addJsonMemberToObject(JSON_MEMBER *member, JSON_MEMBER *new_member)
{
    ASSERT(member != NULL);
    ASSERT(member->Signature == JSON_MEMBER_SIGNATURE);
    ASSERT(new_member != NULL);
    ASSERT(new_member->Signature == JSON_MEMBER_SIGNATURE);

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
    DOB_NEXT_IS_OBJECT,
    DOB_NEXT_IS_NAMED_ARRAY,
    DOB_NEXT_IS_NESTED_ARRAY,
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
    else if (!dob->Bracket) {
        dob->Order = DOB_NEXT_IS_OBJECT;
        name = dob->Path;
        *(dob->Dot) = 0;
        dob->Path = dob->Dot + 1;
    }
    else if (!dob->Dot) {
        if (dob->Bracket == dob->Path) {
            dob->Order = DOB_NEXT_IS_NESTED_ARRAY;
            name = NULL;
            *(dob->Bracket) = 0;
            dob->Path = dob->Bracket + 1;
            dob->ArrayIndex = parseArrayIndex(dob);
        }
        else {
            dob->Order = DOB_NEXT_IS_NAMED_ARRAY;
            name = dob->Path;
            *(dob->Bracket) = 0;
            dob->Path = dob->Bracket + 1;
            dob->ArrayIndex = parseArrayIndex(dob);
        }
    }
    else if (dob->Bracket > dob->Dot) {
        dob->Order = DOB_NEXT_IS_OBJECT;
        name = dob->Path;
        *(dob->Dot) = 0;
        dob->Path = dob->Dot + 1;
    }
    else {
        if (dob->Bracket == dob->Path) {
            dob->Order = DOB_NEXT_IS_NESTED_ARRAY;
            name = NULL;
            *(dob->Bracket) = 0;
            dob->Path = dob->Bracket + 1;
            dob->ArrayIndex = parseArrayIndex(dob);
        }
        else {
            dob->Order = DOB_NEXT_IS_NAMED_ARRAY;
            name = dob->Path;
            *(dob->Bracket) = 0;
            dob->Path = dob->Bracket + 1;
            dob->ArrayIndex = parseArrayIndex(dob);
        }
    }

    return name;
}


JSON_VALUE *findJsonValueInArray(JSON_MEMBER *member, int index)
{
    //------------------------
    int i = 0;
    //------------------------

    while (i < index) {
        member = member->Next;
        if (!member)
            return NULL;
        i++;
    }

    return member->Value;
}


static JSON_VALUE *findJsonValue(char *path, JSON_MEMBER *member)
{
    //------------------------
    JSON_VALUE *value;
    char *name;
    char *path_copy;
    DotOrBracket dob;
    //------------------------

    path_copy = strdup(path);
    dob.Path = path_copy;

    while (1) {

        name = getNextDotOrBracket(&dob);

        switch(dob.Order) {

            case DOB_LAST_ELEMENT:
                member = findJsonMemberInObject(member, name);
                if (!member) {
                    goto FIND_FAILED;
                }
                else if (member->Value->Type == TYPE_OBJECT) {
                    goto FIND_FAILED;
                }
                else {
                    value = member->Value;
                    goto EXIT;
                }

            case DOB_NEXT_IS_OBJECT:
                member = findJsonMemberInObject(member, name);
                if (!member) {
                    goto FIND_FAILED;
                }
                else if (member->Value->Type != TYPE_OBJECT) {
                    goto FIND_FAILED;
                }
                else {
                    member = member->Value->Object;
                    break;
                }

            case DOB_NEXT_IS_NAMED_ARRAY:
                if (dob.ArrayIndex < 0) {
                    goto FIND_FAILED;
                }

                value = findJsonValueInArray(member->Value->Object, dob.ArrayIndex);
                if (!value)
                    goto FIND_FAILED;

                if (value->Type == TYPE_OBJECT) {
                    member = value->Object;
                    // Skip the next dot.
                    dob.Path++;
                }
                else if (value->Type == TYPE_ARRAY) {
                    // We have nested arrays here.
                    member = value->Object;
                }
                else {
                    //  We have the value. We can leave.
                    goto EXIT;
                }
                break;

            case DOB_NEXT_IS_NESTED_ARRAY:

                if (dob.ArrayIndex < 0) {
                    goto FIND_FAILED;
                }

                value = findJsonValueInArray(member, dob.ArrayIndex);
                if (!value)
                    goto FIND_FAILED;

                if (value->Type == TYPE_OBJECT) {
                    member = value->Object;
                    // Skip the next dot.
                    dob.Path++;
                }
                else if (value->Type == TYPE_ARRAY) {
                    // We have nested arrays here.
                    member = value->Object;
                }
                else {
                    //  We have the value. We can leave.
                    goto EXIT;
                }
                break;

            default:
                break;
        }
    }

EXIT:
    // If we make it here, we found the value.
    free(path_copy);
    return value;

    // We didn't find the value, so cleanup.
FIND_FAILED:
    free(path_copy);
    return NULL;
}


JSON_TYPE JSON_GetType(JSON_OBJECT_HANDLE object, char *path)
{
    //------------------------
    JSON_VALUE *value;
    JSON_MEMBER *member;
    //------------------------

    JSON_Errno = SUCCESS;
    member = object;

    if (!object || member->Signature != JSON_MEMBER_SIGNATURE) {
        JSON_Errno = ERROR_INVALID_OBJECT;
        return TYPE_UNKNOWN;
    }

    value = findJsonValue(path, member);

    if (value != NULL)
        return value->Type;
    else
        return TYPE_UNKNOWN;
}


int JSON_GetBoolean(JSON_OBJECT_HANDLE object, char *path)
{
    //------------------------
    JSON_VALUE *value;
    JSON_MEMBER *member;
    //------------------------

    JSON_Errno = SUCCESS;
    member = object;

    if (!object || member->Signature != JSON_MEMBER_SIGNATURE) {
        JSON_Errno = ERROR_INVALID_OBJECT;
        return 0;
    }

    value = findJsonValue(path, object);

    if ((value != NULL) && (value->Type == TYPE_BOOLEAN)) {
        return value->Boolean;
    }
    else {
        JSON_Errno = ERROR_INVALID_BOOLEAN;
        return -ERROR_INVALID_BOOLEAN;
    }
}


double JSON_GetNumber(JSON_OBJECT_HANDLE object, char *path)
{
    //------------------------
    JSON_VALUE *value;
    JSON_MEMBER *member;
    //------------------------

    JSON_Errno = SUCCESS;
    member = object;

    if (!object || member->Signature != JSON_MEMBER_SIGNATURE) {
        JSON_Errno = ERROR_INVALID_OBJECT;
        return -1;
    }

    value = findJsonValue(path, member);

    if ((value != NULL) && (value->Type == TYPE_NUMBER)) {
        return value->Number;
    }
    else {
        JSON_Errno = ERROR_INVALID_NUMBER;
        return 0;
    }
}


char *JSON_GetString(JSON_OBJECT_HANDLE object, char *path)
{
    //------------------------
    JSON_VALUE *value;
    JSON_MEMBER *member;
    //------------------------

    JSON_Errno = SUCCESS;
    member = object;

    if (!object || member->Signature != JSON_MEMBER_SIGNATURE) {
        JSON_Errno = ERROR_INVALID_OBJECT;
        return NULL;
    }

    value = findJsonValue(path, member);

    if ((value != NULL) && (value->Type == TYPE_STRING)) {
        return value->String;
    }
    else {
        JSON_Errno = ERROR_INVALID_STRING;
        return NULL;
    }
}


JSON_OBJECT_HANDLE JSON_GetObject(JSON_OBJECT_HANDLE object, char *path)
{
    //------------------------
    JSON_VALUE *value;
    JSON_MEMBER *member;
    //------------------------

    JSON_Errno = SUCCESS;
    member = object;

    if (!path) {
        JSON_Errno = ERROR_INVALID_JSON_PATH;
        return NULL;
    }

    if (!object || member->Signature != JSON_MEMBER_SIGNATURE) {
        JSON_Errno = ERROR_INVALID_OBJECT;
        return NULL;
    }

    value = findJsonValue(path, member);

    if ((value != NULL) && (value->Type == TYPE_OBJECT)){
        return value->Object;
    }
    else {
        JSON_Errno = ERROR_INVALID_OBJECT;
        return NULL;
    }
}


JSON_OBJECT_HANDLE JSON_AllocObject(void)
{
    //---------------------------
    JSON_MEMBER *member;
    //---------------------------

    JSON_Errno = SUCCESS;

    member = allocJsonMember();

    return member;
}


static JSON_ERROR jsonAddValue(JSON_MEMBER *root_member, char *path, JSON_VALUE *value)
{
    //-----------------------------
    char *name1;
    char *name2;
    char *path_copy;
    int member_should_be_object;
    JSON_ERROR rc = SUCCESS;
    JSON_MEMBER *found_member;
    JSON_MEMBER *new_member;
    //-----------------------------

    ASSERT(root_member->Signature == JSON_MEMBER_SIGNATURE);

    path_copy = strdup(path);

    name1 = strtok(path_copy, ".");

    do {
        name2 = strtok(NULL, ".");
        if (name2)
            member_should_be_object = 1;
        else
            member_should_be_object = 0;

        //  We need to special case the first one, since we'll have an
        //  empty Member at first.
        if (!root_member->Name && !root_member->Value) {

            root_member->Name = strdup(name1);

            if (member_should_be_object) {
                root_member->Value->Type = TYPE_OBJECT;
                root_member->Value->Object = JSON_AllocObject();
                name1 = name2;
                root_member  = new_member;
                continue;
            }
            else {
                root_member->Value = value;
                break;
            }
        }

        found_member = findJsonMemberInObject(root_member, name1);
        if (!found_member) {

            new_member = allocJsonMember();
            if (!new_member) {
                rc = ERROR_ALLOC_FAILED;
                break;
            }
            new_member->Name = strdup(name1);

            addJsonMemberToObject(root_member, new_member);
            if (member_should_be_object) {
                new_member->Value = allocJsonValue();
                new_member->Value->Type = TYPE_OBJECT;
                new_member->Value->Object = allocJsonMember();
                new_member->Value->Object->Name = strdup(name2);
                name1 = name2;
                root_member  = new_member->Value->Object;
            }
            else {
                new_member->Value = value;
                break;
            }
        }
        // This is a value and there isn't any value in it.
        else if (!member_should_be_object && !found_member->Value) {
            found_member->Value = value;
            break;
        }
        else if (!member_should_be_object &&
                  (found_member->Value->Type == value->Type)) {
            free(found_member->Value);
            found_member->Value = value;
            break;
        }
        else if (member_should_be_object &&
                 (found_member->Value->Type == TYPE_OBJECT)) {
            name1 = name2;
            root_member = found_member->Value->Object;
        }
        else {
            rc = ERROR_TYPE_MISMATCH;
            break;
        }

    } while (1);

    free(path_copy);

    return rc;
}


JSON_ERROR JSON_AddBoolean(JSON_OBJECT_HANDLE object, char *path, int value)
{
    //-----------------------------
    JSON_VALUE *json_value;
    JSON_ERROR rc = SUCCESS;
    JSON_MEMBER *member;
    //-----------------------------

    JSON_Errno = SUCCESS;
    member = object;

    if (!path) {
        JSON_Errno = ERROR_INVALID_JSON_PATH;
        return ERROR_INVALID_JSON_PATH;
    }

    if (!object || member->Signature != JSON_MEMBER_SIGNATURE) {
        JSON_Errno = ERROR_INVALID_OBJECT;
        return JSON_Errno;
    }

    json_value = allocJsonValue();
    if (!json_value) {
        JSON_Errno = ERROR_ALLOC_FAILED;
        return JSON_Errno;
    }

    json_value->Type = TYPE_BOOLEAN;
    json_value->Boolean = value ? 1 : 0;

    rc = jsonAddValue(object, path, json_value);

    return rc;
}


JSON_ERROR JSON_AddString(JSON_OBJECT_HANDLE object, char *path, char *value)
{
    //-----------------------------
    JSON_VALUE *json_value;
    JSON_ERROR rc = SUCCESS;
    JSON_MEMBER *member;
    //-----------------------------

    JSON_Errno = SUCCESS;
    member = object;

    if (!path) {
        JSON_Errno = ERROR_INVALID_JSON_PATH;
        return ERROR_INVALID_JSON_PATH;
    }

    if (!object || member->Signature != JSON_MEMBER_SIGNATURE) {
        JSON_Errno = ERROR_INVALID_OBJECT;
        return JSON_Errno;
    }

    json_value = allocJsonValue();
    if (!json_value) {
        JSON_Errno = ERROR_ALLOC_FAILED;
        return JSON_Errno;
    }

    json_value->Type = TYPE_STRING;
    json_value->String = strdup(value);

    rc = jsonAddValue(object, path, json_value);

    return rc;
}


JSON_ERROR JSON_AddNumber(JSON_OBJECT_HANDLE object, char *path, double value)
{
    //-----------------------------
    JSON_VALUE *json_value;
    JSON_ERROR rc = SUCCESS;
    JSON_MEMBER *member;
    //-----------------------------

    JSON_Errno = SUCCESS;
    member = object;

    if (!path) {
        JSON_Errno = ERROR_INVALID_JSON_PATH;
        return ERROR_INVALID_JSON_PATH;
    }

    if (!object || member->Signature != JSON_MEMBER_SIGNATURE) {
        JSON_Errno = ERROR_INVALID_OBJECT;
        return JSON_Errno;
    }

    json_value = allocJsonValue();
    if (!json_value) {
        JSON_Errno = ERROR_ALLOC_FAILED;
        return JSON_Errno;
    }

    json_value->Type = TYPE_NUMBER;
    json_value->Number = value;

    rc = jsonAddValue(object, path, json_value);

    return rc;
}


#ifdef JSON_DBG_PRINT

// Forward declarations
static void dbgPrintJsonValue(JSON_VALUE *value, int *indent_level);
static void dbgPrintJsonObject(JSON_MEMBER *member, int *indent_level);


static void dbgPrintJsonArray(JSON_MEMBER *member, int *indent_level)
{
    ASSERT(member->Signature == JSON_MEMBER_SIGNATURE);

    printIndent(*indent_level);
    printf("ARRAY [\n");
    (*indent_level)++;

    while (member) {
        dbgPrintJsonValue(member->Value, indent_level);
        member = member->Next;
    }

    (*indent_level)--;
    printIndent(*indent_level);
    printf("]\n");
}


static void dbgPrintJsonValue(JSON_VALUE *value, int *indent_level)
{
    ASSERT(value->Signature == JSON_VALUE_SIGNATURE);

    printIndent(*indent_level);
    dbgPrintType(value->Type);

    switch (value->Type) {

        case TYPE_OBJECT:
            dbgPrintJsonObject(value->Object, indent_level);
            break;

        case TYPE_ARRAY:
            dbgPrintJsonArray(value->Object, indent_level);
            break;

        case TYPE_STRING:
            printIndent(*indent_level);
            printf("String: %s\n", value->String);
            break;

        case TYPE_BOOLEAN:
            printIndent(*indent_level);
            if (value->Boolean)
                printf("Boolean: true\n");
            else
                printf("Boolean: false\n");
            break;

        case TYPE_NUMBER:
            printIndent(*indent_level);
            printf("Number: %f\n", value->Number);
            break;

        default:
            break;
    }
}


static void dbgPrintJsonMember(JSON_MEMBER *member, int *indent_level)
{
    printIndent(*indent_level);
    printf("Name: %s\n", member->Name);
    dbgPrintJsonValue(member->Value, indent_level);
    printf("\n");
}


static void dbgPrintJsonObject(JSON_MEMBER *member, int *indent_level)
{
    ASSERT(member->Signature == JSON_MEMBER_SIGNATURE);

    printIndent(*indent_level);
    printf("OBJECT {\n");
    (*indent_level)++;

    while (member) {
        dbgPrintJsonMember(member, indent_level);
        member = member->Next;
    }
    (*indent_level)--;
    printIndent(*indent_level);
    printf("}");
}


void JSONDBG_Print(JSON_OBJECT_HANDLE object)
{
    //--------------------------
    int indent_level = 0;
    JSON_MEMBER *member;
    //--------------------------

    JSON_Errno = SUCCESS;
    member = object;

    if (!object || member->Signature != JSON_MEMBER_SIGNATURE)
        JSON_Errno = ERROR_INVALID_OBJECT;

    dbgPrintJsonObject(member, &indent_level);
    printf("\n");
}

#endif
