#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define ASSERT(_test_condition)                                 \
       if (!(_test_condition)){                                 \
           printf("ASSERT FAILED! \"%s\" %s:%d\n",          	\
                        #_test_condition, __FILE__, __LINE__);  \
           abort();                                             \
       }

/* Forward decl's */
struct _JSON_OBJECT;
struct _JSON_ARRAY;

static struct _JSON_OBJECT* parseJsonObject(char **cursor);

typedef enum _JSON_TYPE {
    TYPE_OBJECT, TYPE_STRING, TYPE_BOOLEAN, TYPE_ARRAY, TYPE_NUMBER
} JSON_TYPE;

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

static void skipBlanks(char **cursor) {
    while (isblank(**cursor)) {
        (*cursor)++;
    }
}

static char* parseJsonString(char **cursor) {
    char *start = NULL;
    char *end = NULL;

    /* Find the first " */
    start = strstr(*cursor, "\"");
    ASSERT(start != NULL);
    start++;

    /* Find the last \" and null terminate the string */
    end = strstr(start, "\"");
    ASSERT(end != NULL);

    *end = 0;
    *cursor = end + 1;

    return start;
}

static int parseJsonBoolean(char **cursor) {
    if (strncmp(*cursor, "true", 4) == 0) {
        *cursor += 4;
        return 1;
    } else if (strncmp(*cursor, "false", 5) == 0) {
        *cursor += 5;
        return 0;
    } else {
        ASSERT(!"Bad Boolean");
    }
}

static double parseJsonNumber(char **cursor) {
    char *end = NULL;
    double value = strtod(*cursor, &end);
    *cursor = end;
    return value;
}

static JSON_VALUE* parseJsonValue(char **cursor) {
    char *start = NULL;
    JSON_VALUE *value;

    value = (JSON_VALUE*) malloc(sizeof(JSON_VALUE));
    ASSERT(value != NULL);
    memset(value, 0, sizeof(JSON_VALUE));

    /* Find the next non-space character */
    skipBlanks(cursor);

    start = *cursor;

    if (*start == '"') {
        value->Type = TYPE_STRING;
        value->String = parseJsonString(cursor);
    } else if (*start == 't') {
        value->Type = TYPE_BOOLEAN;
        value->Boolean = parseJsonBoolean(cursor);
    } else if (*start == 'f') {
        value->Type = TYPE_BOOLEAN;
        value->Boolean = parseJsonBoolean(cursor);
    } else if (isdigit(*start)) {
        value->Type = TYPE_NUMBER;
        value->Number = parseJsonNumber(cursor);
    } else if (*start == '[') {
        value->Type = TYPE_ARRAY;
        //value->String = parseJsonNumber(cursor);
    } else if (*start == '{') {
        value->Type = TYPE_OBJECT;
        value->Object = parseJsonObject(cursor);
    } else {
        ASSERT(!"Unknown value type");
    }

    return value;
}

static JSON_MEMBER* parseJsonMember(char **cursor) {
    JSON_MEMBER *member;

    member = (JSON_MEMBER*) malloc(sizeof(JSON_MEMBER));
    ASSERT(member != NULL);

    /* Get the name */
    member->Name = parseJsonString(cursor);

    /* Find the : */
    *cursor = strstr(*cursor, ":");
    ASSERT((*cursor) != NULL);
    /// Skip past the :
    (*cursor)++;

    /* Get the value */
    member->Value = parseJsonValue(cursor);
    member->Next = NULL;

    return member;
}

static JSON_OBJECT* parseJsonObject(char **cursor) {
    JSON_OBJECT *object;
    JSON_MEMBER *member;
    JSON_MEMBER *prior_member = NULL;
    int parsing_in_progress;

    object = (JSON_OBJECT*) malloc(sizeof(JSON_OBJECT));
    ASSERT(object != NULL);
    memset(object, 0, sizeof(JSON_OBJECT));

    *cursor = strstr(*cursor, "{");
    ASSERT((*cursor) != NULL);
    // Skip past the {
    (*cursor)++;

    do {
        member = parseJsonMember(cursor);
        if (!object->Member) {
            object->Member = member;
        } else {
            prior_member->Next = member;
        }
        prior_member = member;

        /* Find the next non-space character */
        skipBlanks(cursor);

        if (**cursor == ',') {
            parsing_in_progress = 1;
            (*cursor)++;
        } else if (**cursor == '}') {
            parsing_in_progress = 0;
            (*cursor)++;
        } else {
            ASSERT(!"Badly formed Object");
        }

    } while (parsing_in_progress);

    return object;
}

JSON_OBJECT* JSON_Parse(char *string) {
    JSON_OBJECT *object;

    object = parseJsonObject(&string);

    return object;
}

#define SPACES_PER_INDENTATION 4
int gIndentLevel = 0;

static void printIndent(void) {
    int i, j;
    for (i = 0; i < gIndentLevel; i++)
        for (j = 0; j < SPACES_PER_INDENTATION; j++)
            printf(" ");

}

// Forward dec'l
static void printJsonObject(JSON_OBJECT *object);

static void printJsonValue(JSON_VALUE *value) {
    switch (value->Type) {

    case TYPE_OBJECT:
        printJsonObject(value->Object);
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

static void printJsonMember(JSON_MEMBER *member) {
    printIndent();
    printf("\"%s\":", member->Name);
    printJsonValue(member->Value);
}

static void printJsonObject(JSON_OBJECT *object) {
    JSON_MEMBER *member;
    printf("{\n");
    gIndentLevel++;
    member = object->Member;
    while (member) {
        printJsonMember(member);
        member = member->Next;
        if (member) {
            printf(",\n");
        }
    }
    gIndentLevel--;
    printf("\n");
    printIndent();
    printf("}\n");
}

void JSON_Print(JSON_OBJECT *object) {
    printJsonObject(object);
}

// Forward dec'l
static char* stringifyJsonObject(JSON_OBJECT *object, char *buffer,
        int buffer_length);

static void stringifyJsonValue(JSON_VALUE *value) {
    switch (value->Type) {

    case TYPE_OBJECT:
        //stringifyJsonObject(value->Object);
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

static void stringifyJsonMember(JSON_MEMBER *member) {
    printf("\"%s\":", member->Name);
    stringifyJsonValue(member->Value);
}

static char* getBuffer(char *buffer, int *buffer_length, int length) {
    //----------------------
    char *new_buffer;
    int new_length = -1;
    int remaining_length;
    //----------------------

    if (*buffer_length <= 0) {
        new_length = 1024;
    } else {
        remaining_length = (*buffer_length) - length;

        if (remaining_length < 512) {
            new_length = (*buffer_length) + 1024;
        }
    }

    if (new_length > 0) {
        new_buffer = (char*) realloc(buffer, new_length);
        *buffer_length = new_length;
        return new_buffer;
    } else {
        return buffer;
    }
}

static char* stringifyJsonObject(JSON_OBJECT *object, char *buffer,
        int *buffer_length, int *length) {
    //------------------------
    JSON_MEMBER *member;
    //------------------------

    buffer = getBuffer(buffer, buffer_length, *length) * length += sprintf(
            buffer + *length, "{");
    member = object->Member;
    while (member) {
        stringifyJsonMember(member);
        member = member->Next;
        if (member) {
            *length += sprintf(buffer + *length, ",");
        }
    }
    *length += sprintf(buffer + *length, "}");

    return buffer;
}

char* JSON_Stringify(JSON_OBJECT *object) {
    int length = 0;
    char *buffer = stringifyJsonObject(object, NULL, 0, &length);
    return buffer;
}

int main(void) {
    char test1[] =
            "{ \"x\" : true, \"y\": 123.456,\"z\": false, \"w\"\t:\"Hello World\"}";
    char test2[] =
            "{ \"x\" : true, \"y\": {\"y1\": \"Hello\", \"y2\" : \"World\"  }  }";
    char test3[] =
            "{ \"x1\" : {\"x2\" : 111, \"y2\" : {\"x3\":100, \"y3\":123, \"z3\":23}},"
                    " \"y1\": {\"x2\": 1234, \"y2\" : 765  }  }";

    JSON_OBJECT *object;

    printf("JSON TEST\n");

    object = JSON_Parse(test1);
    JSON_Print(object);

    object = JSON_Parse(test2);
    JSON_Print(object);

    object = JSON_Parse(test3);
    JSON_Print(object);

    return 0;
}

