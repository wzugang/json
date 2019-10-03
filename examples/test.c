#include "../json.h"

void doit(char *text)
{
	char *out;json_ht json;
	
	json=json_parse(text);
	if (!json) {printf("Error before: [%s]\n",json_error_get());}
	else
	{
		out=json_print(json,1);
		printf("%s\n",out);
		free(out);
	}
	json_delete(json);
}

void dofile(char *filename)
{
	FILE *f;long len;char *data;	
	f=fopen(filename,"rb");fseek(f,0,SEEK_END);len=ftell(f);fseek(f,0,SEEK_SET);
	data=(char*)malloc(JALIGN(len+1,4));fread(data,1,len,f);fclose(f);
	doit(data);
	free(data);
}

struct record 
{
	const char *precision;
	double lat,lon;
	const char *address,*city,*state,*zip,*country; 
};

void create_objects()
{
	json_ht root,fmt,img,thm,fld;char *out;int i;
	const char *strings[7]={"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};
	
	int numbers[3][3]={{0,-1,0},{1,0,0},{0,0,1}};
	int ids[4]={116,943,234,38793};
	struct record fields[2]=
	{
		{"zip",37.7668,-1.223959e+2,"","SAN FRANCISCO","CA","94107","US"},
		{"zip",37.371991,-1.22026e+2,"","SUNNYVALE","CA","94085","US"}
	};

	root=json_object_new();	
	json_object_add(root, "name", json_string_new("Jack (\"Bee\") Nimble"));
	json_object_add(root, "format", fmt=json_object_new());
	json_object_add(fmt,"type",json_string_new("rect"));
	json_object_add(fmt,"width",json_number_new(1920));
	json_object_add(fmt,"height",json_number_new(1080));
	json_object_add (fmt,"interlace",json_false_new());
	json_object_add(fmt,"frame rate",json_number_new(24));
	
	out=json_print(root,1);	
	json_delete(root);	
	printf("%s\n",out);	
	free(out);

	root=json_array_string_new(strings,7);
	out=json_print(root,1);	
	json_delete(root);	
	printf("%s\n",out);	
	free(out);

	root=json_array_new();
	for (i=0;i<3;i++)
	{
		json_array_add(root,json_array_int_new(numbers[i],3));
	}
	out=json_print(root,1);	
	json_delete(root);	
	printf("%s\n",out);	
	free(out);

	root=json_object_new();
	json_object_add(root, "Image", img=json_object_new());
	json_object_add(img,"Width",json_number_new(800));
	json_object_add(img,"Height",json_number_new(600));
	json_object_add(img,"Title",json_string_new("View from 15th Floor"));
	json_object_add(img, "Thumbnail", thm=json_object_new());
	json_object_add(thm, "Url", json_string_new("http:/*www.example.com/image/481989943"));
	json_object_add_number(thm,"Height",125);
	json_object_add(thm,"Width",json_string_new("100"));
	json_object_add(img,"IDs", json_array_int_new(ids,4));
	out=json_print(root,1);	
	json_delete(root);	
	printf("%s\n",out);	
	free(out);

	root=json_array_new();
	for (i=0;i<2;i++)
	{
		json_array_add(root,fld=json_object_new());
		json_object_add(fld, "precision", json_string_new(fields[i].precision));
		json_object_add(fld, "Latitude", json_number_new(fields[i].lat));
		json_object_add(fld, "Longitude", json_number_new(fields[i].lon));
		json_object_add(fld, "Address", json_string_new(fields[i].address));
		json_object_add(fld, "City", json_string_new(fields[i].city));
		json_object_add(fld, "State", json_string_new(fields[i].state));
		json_object_add(fld, "Zip", json_string_new(fields[i].zip));
		json_object_add(fld, "Country", json_string_new(fields[i].country));
	}	
	//json_object_replace(json_array_get(root,1),"City",json_int_array(ids,4));	
	out=json_print(root,1);	
	json_delete(root);	
	printf("%s\n",out);	
	free(out);

}

void test()
{
	json_ht pJsonRoot = NULL,tf=NULL;

	pJsonRoot = json_object_new();
	if(NULL == pJsonRoot)
	{
		return ;
	}
	json_object_add_string(pJsonRoot, "hello", "hello world");
	json_object_add_number(pJsonRoot, "number", 10010);
	json_object_add_bool(pJsonRoot, "bool", 1);
	json_ht pSubJson = NULL;
	pSubJson = json_object_new();
	if(NULL == pSubJson)
	{
		json_delete(pJsonRoot);
		return ;
	}
	json_object_add_string(pSubJson, "subjsonobj", "a sub json string");
	json_object_add(pJsonRoot, "subobj", pSubJson);
	json_saveto_file(pJsonRoot,"test1.json");
	json_delete(pJsonRoot);
	printf("==========================================json_parse_file====================================================\n");
	
	tf = json_parse_file("test1.json");
	
	char * p = json_print(tf,1);
	if(NULL == p)
	{
		json_delete(tf);
		return ;
	}
	printf("%s\n", p);
	printf("============================================================================================================\n");
	json_delete(tf);
}


void test_child_get(char* file, char* path, char* value)
{
    char* p;
    json_ht jst = NULL;
    json_ht jsh = json_parse_file(file);

    jst = json_child_get(jsh, path);
	if(NULL == jst) {
		return;
	}
	json_ht jv = json_string_new(value);
	json_array_add(jst, jv); //add value without key

    p = json_print(jsh, 1);
    if(NULL != p)
    {
        printf("%s\n", p);
    }
    json_free(p);

	json_object_sort(jsh);
    p = json_print(jsh, 1);
    if(NULL != p)
    {
        printf("%s\n", p);
    }
    json_free(p);
	
	json_bool is_equal = json_compare(jsh, jsh);
	printf("is_equal:%d\n", is_equal);
	
    json_ht jsh2 = json_parse_file(file);
	is_equal = json_compare(jsh, jsh2);
	printf("is_equal:%d\n", is_equal);
	
    json_ht jsh3 = json_parse_file(file);
	is_equal = json_compare(jsh2, jsh3);
	printf("is_equal:%d\n", is_equal);
	
	json_ht jsh4 = json_child_get(jsh, ".subobj.array5:2:0:0.aaa");
	p = json_print(jsh4, 1);
    if(NULL != p)
    {
        printf("json_child_get object item 1:%s\n", p);
    }
    json_free(p);
	
	
	jsh4 = json_child_get(jsh, ".subobj.array5:2:0:0");
	p = json_print(jsh4, 1);
    if(NULL != p)
    {
        printf("json_child_get array item 1:%s\n", p);
    }
    json_free(p);
	
	jsh4 = json_child_get(jsh, ".subobj.array5:2:0:0.");
	p = json_print(jsh4, 1);
    if(NULL != p)
    {
        printf("json_child_get array item 2:%s\n", p);
    }
    json_free(p);
	
	jsh4 = json_child_get(jsh, ".subobj.array5:2:0:0:");
	p = json_print(jsh4, 1);
    if(NULL != p)
    {
        printf("json_child_get array item 3:%s\n", p);
    }
    json_free(p);
	
    json_delete(jsh);
	json_delete(jsh2);
	json_delete(jsh3);
}

typedef struct {
    char name[16];
} Hometown;

typedef struct {
    char id;
    double weight;
    char score[8];
    char name[10];
    Hometown hometown;
} Student;

static void *json_to_struct(json_ht json_obj) {
    /* create Student structure object */
    json_struct_create(struct_student, Student);

    /* deserialize data to Student structure object. */
    json_object_element_get(struct_student, json_obj, int, id);
    json_array_element_get(struct_student, json_obj, int, score);
    json_object_element_get(struct_student, json_obj, string, name);
    json_object_element_get(struct_student, json_obj, double, weight);

    /* deserialize data to Student.Hometown structure object. */
    json_object_struct_element_get(struct_hometown, struct_student, json_hometown, json_obj, Hometown, hometown);
    json_object_element_get(struct_hometown, json_hometown, string, name);

    /* return Student structure object pointer */
    return struct_student;
}

static json_ht struct_to_json(void* struct_obj) {
    Student *struct_student = (Student *)struct_obj;

    /* create Student JSON object */
    json_object_create(json_student);

    /* serialize data to Student JSON object. */
    json_object_element_set(json_student, struct_student, int, id);
    json_object_element_set(json_student, struct_student, double, weight);
    json_array_element_set(json_student, struct_student, int, score, 8);
    json_object_element_set(json_student, struct_student, string, name);

    /* serialize data to Student.Hometown JSON object. */
    json_object_struct_element_set(json_hometown, json_student, struct_hometown, struct_student, Hometown, hometown);
    json_object_element_set(json_hometown, struct_hometown, string, name);

    /* return Student JSON object pointer */
    return json_student;
}

void test_serialize()
{
	char* p=NULL;
	static Student orignal_student_obj = {
            .id = 24,
            .weight = 71.2,
            .score = {1, 2, 3, 4, 5, 6, 7, 8},
            .name = "armink",
            .hometown.name = "China",
    };

    /* serialize Student structure object */
    json_ht json_student = struct_to_json(&orignal_student_obj);
    /* deserialize Student structure object */
    Student *converted_student_obj = json_to_struct(json_student);

    /* compare Student structure object */
    if(memcmp(&orignal_student_obj, converted_student_obj, sizeof(Student))) {
        printf("Converted failed!\n");
    } else {
        printf("Converted OK!\n");
    }
	
	p = json_print(json_student, 1);
    if(NULL != p)
    {
        printf("test_serialize :%s\n", p);
    }
    json_free(p);
	
    json_object_delete(json_student);
    json_struct_delete(converted_student_obj);
}

//json_print,当前只打印值,不打印name
int main (int argc, char * argv[]) 
{
	char text1[]="{\n\"name\": \"Jack (\\\"Bee\\\") Nimble，测试，你好\", \n\"format\": {\"type\":       \"rect\", \n\"width\":      1920, \n\"height\":     1080, \n\"interlace\":  false,\"frame rate\": 24\n}\n}";
	
	doit(text1);

	test();
	
	create_objects();
	
	test_child_get("test_child_get.json", ".subobj.array2", "hehehehheheh");
	
	test_serialize();

	return 0;
}





