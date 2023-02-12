#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include "json.h"


#define ASSERT(_test_condition)                                 \
       if (!(_test_condition)){                                 \
           printf("ASSERT FAILED! \"%s\" %s:%d\n",              \
                        #_test_condition, __FILE__, __LINE__);  \
           abort();                                             \
       }


void test1(void)
{
    //-----------------------------
    JSON_OBJECT_HANDLE object;
    char *buffer;
    int b;
    char test[] =
            "{ \"x\" : true, \"y\": 123.456,\"z\": false, \"w\"\t:\"Hello World\"}";
    //-----------------------------

    printf("\nTEST 1\n----------------------------\n");
    object = JSON_Parse(test);
    ASSERT(JSON_GetErrno() == SUCCESS);

    JSON_Print(object);
    ASSERT(JSON_GetErrno() == SUCCESS);

    JSONDBG_Print(object);

    buffer = JSON_Stringify(object);
    ASSERT(JSON_GetErrno() == SUCCESS);
    printf("%s\n\n", buffer);
    free(buffer);

    b = JSON_GetBoolean(object, "x");
    ASSERT(JSON_GetErrno() == SUCCESS);
    printf("JSON_GetBoolean x = %d\n", b);

    b = JSON_GetBoolean(object, "z");
    ASSERT(JSON_GetErrno() == SUCCESS);
    printf("JSON_GetBoolean x = %d\n", b);

    b = JSON_GetBoolean(object, "unk");
    ASSERT(JSON_GetErrno() != SUCCESS);
    printf("JSON_GetBoolean unk = %d\n", b);

    JSON_FreeObject(object);
    ASSERT(JSON_GetErrno() == SUCCESS);
}


void test2(void)
{
    //-----------------------------
    JSON_OBJECT_HANDLE object;
    char *buffer;
    int b;
    char test[] =
            "{ \"x\" : true, \"y\": {\"y1\": \"Hello\", \"y2\" : \"World\", \"y3\" : true  }  }";
    //-----------------------------

    printf("\nTEST 2\n----------------------------\n");

    object = JSON_Parse(test);
    ASSERT(JSON_GetErrno() == SUCCESS);

    JSON_Print(object);
    ASSERT(JSON_GetErrno() == SUCCESS);

    JSONDBG_Print(object);

    buffer = JSON_Stringify(object);
    ASSERT(JSON_GetErrno() == SUCCESS);
    printf("%s\n\n", buffer);
    free(buffer);

    b = JSON_GetBoolean(object, "y.y3");
    ASSERT(JSON_GetErrno() == SUCCESS);
    printf("JSON_GetBoolean y.y3 = %d\n", b);

    JSON_FreeObject(object);
    ASSERT(JSON_GetErrno() == SUCCESS);
}


void test3(void)
{
    //-----------------------------
    JSON_OBJECT_HANDLE object;
    char *buffer;
    char test[] =
            "{ \"x1\" : {\"x2\" : 111, \"y2\" : {\"x3\":100, \"y3\":123, \"z3\":23}},"
                    " \"y1\": {\"x2\": 1234, \"y2\" : 765  }  }";
    //-----------------------------

    printf("\nTEST 3\n----------------------------\n");

    object = JSON_Parse(test);
    ASSERT(JSON_GetErrno() == SUCCESS);

    JSON_Print(object);
    ASSERT(JSON_GetErrno() == SUCCESS);

    JSONDBG_Print(object);

    buffer = JSON_Stringify(object);
    ASSERT(JSON_GetErrno() == SUCCESS);
    printf("%s\n\n", buffer);
    free(buffer);

    JSON_FreeObject(object);
    ASSERT(JSON_GetErrno() == SUCCESS);
}


void test4(void)
{
    //-----------------------------
    JSON_OBJECT_HANDLE object;
    char *buffer;
    int b;
    char test[] =
            "{ \"A1\" : [1, 2, 3]  }";
    //-----------------------------

    printf("\nTEST 4\n----------------------------\n");

    object = JSON_Parse(test);
    ASSERT(JSON_GetErrno() == SUCCESS);

    JSON_Print(object);
    ASSERT(JSON_GetErrno() == SUCCESS);

    JSONDBG_Print(object);

    buffer = JSON_Stringify(object);
    ASSERT(JSON_GetErrno() == SUCCESS);
    printf("%s\n\n", buffer);
    free(buffer);

    b = JSON_GetNumber(object, "A1[0]");
    ASSERT(JSON_GetErrno() == SUCCESS);
    printf("A1[0] = %d\n", b);

    b = JSON_GetNumber(object, "A1[1]");
    ASSERT(JSON_GetErrno() == SUCCESS);
    printf("A1[1] = %d\n", b);

    b = JSON_GetNumber(object, "A1[2]");
    ASSERT(JSON_GetErrno() == SUCCESS);
    printf("A1[2] = %d\n", b);

    b = JSON_GetNumber(object, "A1[3]");
    printf("A1[3] = %d\n", b);
    ASSERT(JSON_GetErrno() != SUCCESS);

    JSON_FreeObject(object);
    ASSERT(JSON_GetErrno() == SUCCESS);
}


void test5(void)
{
    //---------------------------------
    JSON_OBJECT_HANDLE object;
    char *buffer;
    int b;
    char *string;
    char test[] =
            "{ \"A1\" : [\"Hi A String\", { \"TheAnswer\":42} , [12, 13, 14]]  }";
    //---------------------------------

    printf("\nTEST 5\n----------------------------\n");

    object = JSON_Parse(test);
    ASSERT(JSON_GetErrno() == SUCCESS);

    JSON_Print(object);
    ASSERT(JSON_GetErrno() == SUCCESS);

    JSONDBG_Print(object);

    buffer = JSON_Stringify(object);
    ASSERT(JSON_GetErrno() == SUCCESS);
    printf("%s\n\n", buffer);
    free(buffer);

    string = JSON_GetString(object, "A1[0]");
    ASSERT(JSON_GetErrno() == SUCCESS);
    printf("A1[0] = %s\n", string);

    b = JSON_GetNumber(object, "A1[1].TheAnswer");
    ASSERT(JSON_GetErrno() == SUCCESS);
    printf("A1[1].TheAnswer = %d\n", b);

    b = JSON_GetNumber(object, "A1[2][0]");
    ASSERT(JSON_GetErrno() == SUCCESS);
    printf("A1[2][0] = %d\n", b);

    b = JSON_GetNumber(object, "A1[2][1]");
    ASSERT(JSON_GetErrno() == SUCCESS);
    printf("A1[2][1] = %d\n", b);

    b = JSON_GetNumber(object, "A1[2][2]");
    ASSERT(JSON_GetErrno() == SUCCESS);
    printf("A1[2][2] = %d\n", b);

    JSON_FreeObject(object);
    ASSERT(JSON_GetErrno() == SUCCESS);
}


void test6(void)
{
    //---------------------------------
    JSON_OBJECT_HANDLE object;
    //---------------------------------

    printf("\nTEST 6\n----------------------------\n");

    object = JSON_AllocObject();
    ASSERT(JSON_GetErrno() == SUCCESS);

    JSON_AddBoolean(object, "testBool2", 0);
    ASSERT(JSON_GetErrno() == SUCCESS);
    JSON_AddBoolean(object, "testBool3", 0);
    ASSERT(JSON_GetErrno() == SUCCESS);
    JSON_AddBoolean(object, "testBool1.sub1", 1);
    ASSERT(JSON_GetErrno() == SUCCESS);
    JSON_AddBoolean(object, "testBool1.sub3", 1);
    ASSERT(JSON_GetErrno() == SUCCESS);
    JSON_AddBoolean(object, "testBool1.sub2.x", 1);
    ASSERT(JSON_GetErrno() == SUCCESS);

    JSON_AddString(object, "testBool1.sub4.ProductName", "C Programming");
    ASSERT(JSON_GetErrno() == SUCCESS);

    JSON_AddNumber(object, "testBool1.sub4.ProductCost", 29.95);
    ASSERT(JSON_GetErrno() == SUCCESS);

    JSON_Print(object);
    ASSERT(JSON_GetErrno() == SUCCESS);

    JSONDBG_Print(object);

    JSON_FreeObject(object);
    ASSERT(JSON_GetErrno() == SUCCESS);
}


int main(void) {
    printf("JSON TEST\n");

    test1();
    test2();
    test3();
    test4();
    test5();
    test6();

    return 0;
}

