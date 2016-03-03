#ifndef __W_JSON_H__
#define __W_JSON_H__
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <float.h>
#include <limits.h>
#include <ctype.h>

//#define BUILD_SHARED

#define JWINDOWS 	( defined(WIN32) || defined(_WIN32) || defined(WINNT) || defined(__MINGW32__) || defined(__MINGW64__) )

#if JWINDOWS
#if		defined(__cplusplus)
#if		defined(BUILD_SHARED)
#define 	JAPI 		 __declspec(dllexport)
#else
#define 	JAPI 		 __declspec(dllimport)
#endif
#else
#define 	JAPI 		 
#endif
#else
#if	defined(__GNUC__) &&  __GNUC__ >= 4
#define 	JAPI 		 __attribute__((visibility("default")))
#else
#define 	JAPI 		
#endif
#endif

#if (defined(GCC) || defined(GPP) || defined(__cplusplus))
#define JSTATIC static inline
#else
#define JSTATIC static 
#endif


#ifdef __cplusplus
#define JEXPORT extern "C"
#else
#define JEXPORT extern 
#endif

#define JCHECK(func)\
do {\
int _s = (func);\
if (_s < 0)\
{\
fprintf(stderr, "Error: %s returned %d\n", #func, _s);\
exit(0);\
}\
} while (0)

typedef enum __json_type_t
{
JSON_FALSE,
JSON_TRUE,
JSON_NULL,
JSON_NUMBER,
JSON_STRING,
JSON_ARRAY,
JSON_OBJECT
}json_type_t,*json_type_ht;

//item为引用节点
#define JSON_IS_REFERENCE 		128
//节点名称为常量
#define JSON_IS_STR_CONST		512

//所有数据类型结构相同,占用空间会大一点
typedef struct __json_t
{
	struct __json_t 		*next,*prev;
	struct __json_t 		*child;
	int						type;
	char 					*name;
	char					*valuestring;
	int 					valueint;
	double					valuedouble;
}json_t,*json_ht;

typedef struct __json_hooks_t
{
    void *(*alloc)(size_t size);
    void (*free)(void *p);
}json_hooks_t,*json_hooks_ht;

typedef struct __json_buffer_t
{
	char *buffer; 
	int length; 
	int offset; 
}json_buffer_t,*json_buffer_ht;

/////////////////////////////////////////////////////////////////////////////////////////////////
#define JZERO(op) 			memset(op, 0, sizeof(__typeof__(*op)))
#define JZERO_LEN(op,len) 	memset(op, 0, len)
#define JALIGN(x,align)		(((x) + (align) - 1) & ~((align)-1))


#define IS_NUM(c)		((c) <= '9' && (c) >= '0')

//待解决问题,支持注释

static char hex_table[128]=
{
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	0 , 1, 2, 3, 4, 5, 6, 7, 8, 9,-1,-1,-1,-1,-1,-1,
	-1,10,11,12,13,14,15,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,10,11,12,13,14,15,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
};

JEXPORT JAPI void 								json_hooks_init(json_hooks_ht hooks);
JEXPORT JAPI void*								json_alloc(size_t size);
JEXPORT JAPI void								json_free(void *p);
JEXPORT JAPI json_ht 							json_parse(const char *value);
JEXPORT JAPI json_ht 							json_parse_file(char *filename);
JEXPORT JAPI int  								json_saveto_file(json_ht item,char *filename);
JEXPORT JAPI void 								json_delete(json_ht item);
JEXPORT JAPI char*								json_print(json_ht item,int fmt);
JEXPORT JAPI char*								json_print_buffered(json_ht item,int size,int fmt);
JEXPORT JAPI json_ht 							json_duplicate(json_ht item,int recurse);
JEXPORT JAPI void 								json_minify(char *json);
JEXPORT JAPI const char*						json_error_get(void);
JEXPORT JAPI void 								json_error_clear(void);
					
JEXPORT JAPI json_ht 							json_null_new(void); 
JEXPORT JAPI json_ht 							json_true_new(void); 
JEXPORT JAPI json_ht 							json_false_new(void);
JEXPORT JAPI json_ht 							json_bool_new(int b);
JEXPORT JAPI json_ht 							json_number_new(double num);
JEXPORT JAPI json_ht 							json_string_new(const char *string);
						
JEXPORT JAPI json_ht 							json_array_new(void);
JEXPORT JAPI json_ht 							json_array_int_new(const int *numbers,int count);
JEXPORT JAPI json_ht 							json_array_float_new(const float *numbers,int count);
JEXPORT JAPI json_ht 							json_array_double_new(const double *numbers,int count);
JEXPORT JAPI json_ht 							json_array_string_new(const char **strings,int count);
JEXPORT JAPI void 								json_array_add(json_ht array,json_ht item);
JEXPORT JAPI void 								json_array_insert(json_ht rray,int which,json_ht newitem);
JEXPORT JAPI void 								json_array_replace(json_ht array,int which,json_ht newitem);
JEXPORT JAPI json_ht 							json_array_detach(json_ht array,int which);
JEXPORT JAPI void 								json_array_del(json_ht array,int which);
JEXPORT JAPI void 								json_array_reference_add(json_ht array,json_ht item);
JEXPORT JAPI json_ht 							json_array_get(json_ht array,int item);
JEXPORT JAPI int 								json_array_size(json_ht array);
					
JEXPORT JAPI json_ht 							json_object_new(void);
JEXPORT JAPI json_ht 							json_object_get(json_ht object,const char *string);
JEXPORT JAPI void 								json_object_add(json_ht object,const char *string,json_ht item);
JEXPORT JAPI void 								json_object_add_cs(json_ht object,const char *string,json_ht item);
JEXPORT JAPI void 								json_object_reference_add(json_ht object,const char *string,json_ht item);
JEXPORT JAPI json_ht 							json_object_detach(json_ht object,const char *string);
JEXPORT JAPI void 								json_object_del(json_ht object,const char *string);
JEXPORT JAPI void 								json_object_replace(json_ht object,const char *string,json_ht newitem);

#define json_object_add_null(object,name)		json_object_add(object, name, json_null_new())
#define json_object_add_true(object,name)		json_object_add(object, name, json_true_new())
#define json_object_add_false(object,name)		json_object_add(object, name, json_false_new())
#define json_object_add_bool(object,name,b)		json_object_add(object, name, json_bool_new(b))
#define json_object_add_number(object,name,n)	json_object_add(object, name, json_number_new(n))
#define json_object_add_string(object,name,s)	json_object_add(object, name, json_string_new(s))

#define json_typeof(object)      				((object)->type)
#define json_is_object(object)   				(object && json_typeof(object) == JSON_OBJECT)
#define json_is_array(object)    				(object && json_typeof(object) == JSON_ARRAY)
#define json_is_string(object)   				(object && json_typeof(object) == JSON_STRING)
#define json_is_number(object)   				(object && json_typeof(object) == JSON_NUMBER)
#define json_is_true(object)     				(object && json_typeof(object) == JSON_TRUE)
#define json_is_false(object)    				(object && json_typeof(object) == JSON_FALSE)
#define json_is_null(object)     				(object && json_typeof(object) == JSON_NULL)
#define json_is_bool(object)  					(object_is_true(object) || json_is_false(object))

//整形赋值的时候double也必须赋值
#define json_set_int(object,val)				((object)?(object)->valueint=(object)->valuedouble=(val):(val))
#define json_set_number(object,val)				((object)?(object)->valueint=(object)->valuedouble=(val):(val))


//number整形与浮点型区分,待改善
#define json_is_integer(object)  				(object && json_typeof(object) == JSON_INTEGER)
#define json_is_real(object)     				(object && json_typeof(object) == JSON_REAL)


#endif













