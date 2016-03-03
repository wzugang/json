#include "json.h"

static const char *json_error;

//内存管理函数设置
static void *(*do_alloc)(size_t size)			= malloc;
static void (*do_free)(void *p) 				= free;

void *json_alloc(size_t size)					{ if(!size)return NULL; return (*do_alloc)(size); }
void json_free(void *p) 						{ if(!p)return;(*do_free)(p); }

//跳过小于32的字符
JSTATIC const char *skip(const char *in) {while (in && *in && (unsigned char)*in<=32) in++; return in;}

const char *json_error_get(void) {return json_error;}
void json_error_clear(void) {json_error=NULL;}

JSTATIC char* json_strdup(const char* str)
{
      int len;
      char* copy;
      len = strlen(str) + 1;
      if (!(copy = (char*)json_alloc(len))) return NULL;
      memcpy(copy,str,len);
      return copy;
}

//比较字符串相等返回0
JSTATIC int json_strcasecmp(const char *s1,const char *s2)
{
	if (!s1) return (s1==s2)?0:1;if (!s2) return 1;
	for(; tolower(*s1) == tolower(*s2); ++s1, ++s2)	if(*s1 == 0)	return 0;
	return tolower(*(const unsigned char *)s1) - tolower(*(const unsigned char *)s2);
}

void json_hooks_init(json_hooks_ht hooks)
{
    if (NULL == hooks) { do_alloc = malloc;do_free = free;} else { do_alloc = (hooks->alloc)?hooks->alloc:malloc;do_free = (hooks->free)?hooks->free:free;}
}

JSTATIC json_ht json_new_item(void)
{
	json_ht item = (json_ht)json_alloc(sizeof(json_t));if (item) JZERO(item);return item;
}

void json_delete(json_ht item)
{
	json_ht next;
	while (item)
	{
		next=item->next;
		if (!(item->type & JSON_IS_REFERENCE) && item->child) json_delete(item->child);
		if (!(item->type & JSON_IS_REFERENCE) && item->valuestring) json_free(item->valuestring);
		if (!(item->type & JSON_IS_STR_CONST) && item->name) json_free(item->name);
		json_free(item);
		item=next;
	}
}

JSTATIC unsigned int parse_hex(const char* hex,int n)
{
	unsigned i,h=0;
	for(i=0;i<n;i++)
	{
		if(*hex < 0)return 0;
		if(hex_table[*hex] < 0)return 0;
		h += hex_table[*hex++];h=h<<4;
	}
	return h;
}

JSTATIC const char *parse_value(json_ht item,const char *value);
JSTATIC char *print_value(json_ht item,int depth,int fmt,json_buffer_ht p);

//add\v去掉firstByteMark
JSTATIC const char *parse_string(json_ht item,const char *str)
{
	const char *ptr=str+1;char *ptr2;char *out=NULL;int len=0;unsigned int uc,uc2;
	if (*str!='\"') {json_error=str;return NULL;}
	
	while (*ptr!='\"' && *ptr && ++len) if (*ptr++ == '\\') ptr++;//自动跳过转义字符
	
	if (!(out=(char*)json_alloc(len+1))) return NULL;
	
	ptr=str+1;ptr2=out;
	while (*ptr!='\"' && *ptr)
	{
		if (*ptr!='\\') *ptr2++=*ptr++;
		else
		{
			ptr++;
			switch (*ptr)
			{
				case 'b': *ptr2++='\b';	break;case 'f': *ptr2++='\f'; break;case 'v': *ptr2++='\v'; break;case 'n': *ptr2++='\n'; break;case 'r': *ptr2++='\r'; break;case 't': *ptr2++='\t';	break;
				case 'u'://transcode utf16 to utf8
					uc=parse_hex(ptr+1,4);ptr+=4;	//get the unicode char
					if ((uc>=0xDC00 && uc<=0xDFFF) || uc==0) break;//check for invalid
					if (uc>=0xD800 && uc<=0xDBFF)//UTF16 surrogate pairs
					{
						if (ptr[1]!='\\' || ptr[2]!='u')	break;//missing second-half of surrogate
						uc2=parse_hex(ptr+3,4);ptr+=6;
						if (uc2<0xDC00 || uc2>0xDFFF)		break;//invalid second-half of surrogate
						uc=0x10000 + (((uc&0x3FF)<<10) | (uc2&0x3FF));
					}
					len=4;if (uc<0x80) len=1;else if (uc<0x800) len=2;else if (uc<0x10000) len=3; ptr2+=len;				
					switch (len) 
					{
						case 4: *--ptr2 =((uc | 0x80) & 0xBF); uc >>= 6;
						case 3: *--ptr2 =((uc | 0x80) & 0xBF); uc >>= 6;
						case 2: *--ptr2 =((uc | 0x80) & 0xBF); uc >>= 6;
						case 1: *--ptr2 =(uc);
					}
					ptr2+=len;
					break;
				default:  *ptr2++=*ptr; break;
			}
			ptr++;
		}
	}
	*ptr2=0;
	if (*ptr=='\"') ptr++;
	item->valuestring=out;
	item->type=JSON_STRING;
	return ptr;
}

//解析数字,并返回解析后字符串的偏移指针
JSTATIC const char *parse_number(json_ht item,const char *num)
{
	double n=0,sign=1,scale=0;int subscale=0,signsubscale=1;

	if (*num=='-') sign=-1,num++;//符号
	//if (*num=='0') num++;
	//if (*num>='1' && *num<='9')	do	n=(n*10.0)+(*num++ -'0');	while (*num>='0' && *num<='9');	//数字
	while (IS_NUM(*num))n=(n*10.0)+(*num++ -'0');
	if (*num=='.' && IS_NUM(num[1])) {num++; while (IS_NUM(*num))n=(n*10.0)+(*num++ -'0'),scale--;}	//小数部分
	if (*num=='e' || *num=='E')	//科学计数法
	{	num++;if (*num=='+') num++;	else if (*num=='-') signsubscale=-1,num++;//符号
		while (IS_NUM(*num)) subscale=(subscale*10)+(*num++ - '0');//数字
	}
	n=sign*n*pow(10.0,(scale+subscale*signsubscale));//number = +/- number.fraction * 10^+/- exponent
	item->valuedouble=n;
	item->valueint=(int)n;
	item->type=JSON_NUMBER;
	return num;
}

JSTATIC const char *parse_array(json_ht item,const char *value)
{
	json_ht child;
	if (*value!='[')	{json_error=value;return NULL;}
	item->type=JSON_ARRAY;
	value=skip(value+1);
	if (*value==']') return value+1;

	item->child=child=json_new_item();if (!item->child) return NULL;
	value=skip(parse_value(child,skip(value)));
	if (!value) return NULL;

	while (*value==',')
	{
		json_ht new_item;
		if (!(new_item=json_new_item())) return NULL;
		child->next=new_item;new_item->prev=child;child=new_item;
		value=skip(parse_value(child,skip(value+1)));
		if (!value) return NULL;
	}

	if (*value==']') return value+1;
	json_error=value;return NULL;
}

JSTATIC const char *parse_object(json_ht item,const char *value)
{
	json_ht child;
	if (*value!='{') {json_error=value;return NULL;}
	item->type=JSON_OBJECT;
	value=skip(value+1);
	if (*value=='}') return value+1;
	
	item->child=child=json_new_item();
	if (!item->child) return NULL;
	value=skip(parse_string(child,skip(value)));
	if (!value) return NULL;
	child->name=child->valuestring;child->valuestring=NULL;
	if (*value!=':') {json_error=value;return NULL;}
	value=skip(parse_value(child,skip(value+1)));
	if (!value) return NULL;
	
	while (*value==',')
	{
		json_ht new_item;
		if (!(new_item=json_new_item())) return NULL;
		child->next=new_item;new_item->prev=child;child=new_item;
		value=skip(parse_string(child,skip(value+1)));
		if (!value) return NULL;
		child->name=child->valuestring;child->valuestring=NULL;
		if (*value!=':') {json_error=value;return NULL;}
		value=skip(parse_value(child,skip(value+1)));
		if (!value) return NULL;
	}
	
	if (*value=='}') return value+1;
	json_error=value;return NULL;
}

JSTATIC const char *parse_value(json_ht item,const char *value)
{
	if (!value)						return NULL;
	if (!strncmp(value,"null",4))	{ item->type=JSON_NULL;  return value+4; }
	if (!strncmp(value,"false",5))	{ item->type=JSON_FALSE; return value+5; }
	if (!strncmp(value,"true",4))	{ item->type=JSON_TRUE; item->valueint=1;	return value+4; }
	if (*value=='\"')				{ return parse_string(item,value); }
	if (*value=='-' || (IS_NUM(*value)))	{ return parse_number(item,value); }
	if (*value=='[')				{ return parse_array(item,value); }
	if (*value=='{')				{ return parse_object(item,value); }

	json_error=value;return NULL;
}

JSTATIC json_ht json_parse_opts(const char *value,const char **return_parse_end,int require_null_terminated)
{
	const char *end=NULL;json_error=NULL;
	json_ht item=json_new_item();
	if (!item) return NULL;end=parse_value(item,skip(value));
	if (!end) {json_delete(item);return NULL;}	//解析失败
	if (require_null_terminated) {end=skip(end);if (*end) {json_delete(item);json_error=end;return NULL;}}//解析结束地址为null,终止
	if (return_parse_end) *return_parse_end=end;return item;//返回解析结束的地址
}

json_ht json_parse(const char *value) {return json_parse_opts(value,NULL,0);}

//内存扩展计算函数
//JSTATIC int pow2gt (int x) { --x; x|=x>>1; x|=x>>2;	x|=x>>4; x|=x>>8; x|=x>>16;	return x+1;	}
JSTATIC int string_expand(int x) { return JALIGN(x,4);}

//检查内存大小,如果不够动态增加内存
JSTATIC char* string_ensure(json_buffer_ht p,int needed)
{
	char *newbuffer;int newsize;
	if (!p || !p->buffer) return NULL;
	needed+=p->offset;
	if (needed<=p->length) return p->buffer+p->offset;
	newsize=string_expand(needed*2);
	newbuffer=(char*)json_alloc(newsize);
	if (!newbuffer) {json_free(p->buffer);p->length=0,p->buffer=NULL;return NULL;}
	if (newbuffer) memcpy(newbuffer,p->buffer,p->length);
	json_free(p->buffer);
	p->length=newsize;
	p->buffer=newbuffer;
	return newbuffer+p->offset;
}

JSTATIC int json_buffer_update(json_buffer_ht p)
{
	char *str;
	if (!p || !p->buffer) return 0;
	str=p->buffer+p->offset;
	return p->offset+strlen(str);
}

JSTATIC char *print_number(json_ht item,json_buffer_ht p)
{
	char *str=NULL;
	double d=item->valuedouble;
	if (d==0)
	{
		if (p) str=string_ensure(p,2);
		else str=(char*)json_alloc(2);
		if (str) strcpy(str,"0");
	}
	else if (fabs(((double)item->valueint)-d)<=DBL_EPSILON && d<=INT_MAX && d>=INT_MIN)
	{
		if (p) str=string_ensure(p,21);
		else str=(char*)json_alloc(21);
		if (str) sprintf(str,"%d",item->valueint);
	}
	else
	{
		if (p) str=string_ensure(p,64);
		else str=(char*)json_alloc(64);
		if (str)
		{
			if (fabs(floor(d)-d)<=DBL_EPSILON && fabs(d)<1.0e60)sprintf(str,"%.0f",d);
			else if (fabs(d)<1.0e-6 || fabs(d)>1.0e9) sprintf(str,"%e",d);
			else sprintf(str,"%f",d);
		}
	}
	return str;
}

JSTATIC char *print_valuestring(const char *str,json_buffer_ht p)
{
	const char *ptr;char *ptr2,*out;int len=0,flag=0;unsigned char token;
	
	for (ptr=str;*ptr;ptr++) flag|=((*ptr>0 && *ptr<32)||(*ptr=='\"')||(*ptr=='\\'))?1:0;
	if (!flag)
	{
		len=ptr-str;
		if (p) out=string_ensure(p,len+3);
		else out=(char*)json_alloc(len+3);
		if (!out) return 0;
		ptr2=out;*ptr2++='\"';
		strcpy(ptr2,str);
		ptr2[len]='\"';
		ptr2[len+1]=0;
		return out;
	}
	
	if (!str)
	{
		if (p) out=string_ensure(p,3);
		else out=(char*)json_alloc(3);
		if (!out) return 0;
		strcpy(out,"\"\"");
		return out;
	}
	ptr=str;while ((token=*ptr) && ++len) {if (strchr("\"\\\b\f\n\r\t",token)) len++; else if (token<32) len+=5;ptr++;}
	
	if (p) out=string_ensure(p,len+3);
	else out=(char*)json_alloc(len+3);
	if (!out) return 0;

	ptr2=out;ptr=str;
	*ptr2++='\"';
	while (*ptr)
	{
		if ((unsigned char)*ptr>31 && *ptr!='\"' && *ptr!='\\') *ptr2++=*ptr++;
		else
		{
			*ptr2++='\\';
			switch (token=*ptr++)
			{
				case '\\':	*ptr2++='\\'; break;
				case '\"':	*ptr2++='\"'; break;
				case '\b':	*ptr2++='b'; break;
				case '\v':	*ptr2++='v'; break;
				case '\f':	*ptr2++='f'; break;
				case '\n':	*ptr2++='n'; break;
				case '\r':	*ptr2++='r'; break;
				case '\t':	*ptr2++='t'; break;
				default: sprintf(ptr2,"u%04x",token);ptr2+=5; break;
			}
		}
	}
	*ptr2++='\"';*ptr2++=0;
	return out;
}

JSTATIC char *print_string(json_ht item,json_buffer_ht p) {return print_valuestring(item->valuestring,p);}

JSTATIC char *print_array(json_ht item,int depth,int fmt,json_buffer_ht p)
{
	char **entries;
	char *out=0,*ptr,*ret;int len=5;
	json_ht child=item->child;
	int numentries=0,i=0,fail=0;
	int tmplen=0;
	
	while (child) numentries++,child=child->next;
	if (!numentries)
	{
		if (p) out=string_ensure(p,3);
		else out=(char*)json_alloc(3);
		if (out) strcpy(out,"[]");
		return out;
	}

	if (p)
	{
		i=p->offset;
		ptr=string_ensure(p,1);if (!ptr) return 0;	*ptr='[';	p->offset++;
		child=item->child;
		while (child && !fail)
		{
			print_value(child,depth+1,fmt,p);
			p->offset=json_buffer_update(p);
			if (child->next) {len=fmt?2:1;ptr=string_ensure(p,len+1);if (!ptr) return 0;*ptr++=',';if(fmt)*ptr++=' ';*ptr=0;p->offset+=len;}
			child=child->next;
		}
		ptr=string_ensure(p,2);if (!ptr) return 0;	*ptr++=']';*ptr=0;
		out=(p->buffer)+i;
	}
	else
	{
		entries=(char**)json_alloc(numentries*sizeof(char*));
		if (!entries) return 0;
		memset(entries,0,numentries*sizeof(char*));
		child=item->child;
		while (child && !fail)
		{
			ret=print_value(child,depth+1,fmt,0);
			entries[i++]=ret;
			if (ret) len+=strlen(ret)+2+(fmt?1:0); else fail=1;
			child=child->next;
		}
		
		if (!fail) out=(char*)json_alloc(len);
		if (!out) fail=1;

		if (fail)
		{
			for (i=0;i<numentries;i++) if (entries[i]) json_free(entries[i]);
			json_free(entries);
			return 0;
		}
		*out='[';
		ptr=out+1;*ptr=0;
		for (i=0;i<numentries;i++)
		{
			tmplen=strlen(entries[i]);memcpy(ptr,entries[i],tmplen);ptr+=tmplen;
			if (i!=numentries-1) {*ptr++=',';if(fmt)*ptr++=' ';*ptr=0;}
			json_free(entries[i]);
		}
		json_free(entries);
		*ptr++=']';*ptr++=0;
	}
	return out;	
}

JSTATIC char *print_object(json_ht item,int depth,int fmt,json_buffer_ht p)
{
	char **entries=0,**names=0;
	char *out=0,*ptr,*ret,*str;int len=7,i=0,j;
	json_ht child=item->child;
	int numentries=0,fail=0;
	size_t tmplen=0;
	while (child) numentries++,child=child->next;
	if (!numentries)
	{
		if (p) out=string_ensure(p,fmt?depth+4:3);
		else out=(char*)json_alloc(fmt?depth+4:3);
		if (!out) return 0;
		ptr=out;*ptr++='{';
		if (fmt) {*ptr++='\n';for (i=0;i<depth-1;i++) *ptr++='\t';}
		*ptr++='}';*ptr++=0;
		return out;
	}
	if (p)
	{
		i=p->offset;
		len=fmt?2:1; ptr=string_ensure(p,len+1); if (!ptr) return 0;
		*ptr++='{';	if (fmt) *ptr++='\n'; *ptr=0; p->offset+=len;
		child=item->child;depth++;
		while (child)
		{
			if (fmt)
			{
				ptr=string_ensure(p,depth);	if (!ptr) return 0;
				for (j=0;j<depth;j++) *ptr++='\t';
				p->offset+=depth;
			}
			print_valuestring(child->name,p);
			p->offset=json_buffer_update(p);
			
			//if(child->child)
			//	len=fmt?depth+1:1;
			//else
				len=fmt?2:1;
			ptr=string_ensure(p,len); if (!ptr) return 0;
			*ptr++=':';if (fmt) *ptr++='\t';
			//*ptr++=':';if (fmt) *ptr++='\n';
			//if(child->child && (json_is_array(child) || json_is_object(child)))		//add
			//if(child->child)										//add
			//	if (fmt) for (i=0;i<depth-1;i++) *ptr++='\t';		//add
			p->offset+=len;
			
			print_value(child,depth,fmt,p);
			p->offset=json_buffer_update(p);

			len=(fmt?1:0)+(child->next?1:0);
			ptr=string_ensure(p,len+1); if (!ptr) return 0;
			if (child->next) *ptr++=',';
			if (fmt) *ptr++='\n';*ptr=0;
			p->offset+=len;
			child=child->next;
		}
		ptr=string_ensure(p,fmt?(depth+1):2);	 if (!ptr) return 0;
		if (fmt)	for (i=0;i<depth-1;i++) *ptr++='\t';
		*ptr++='}';*ptr=0;
		out=(p->buffer)+i;
	}
	else
	{
		entries=(char**)json_alloc(numentries*sizeof(char*));
		if (!entries) return 0;
		names=(char**)json_alloc(numentries*sizeof(char*));
		if (!names) {json_free(entries);return 0;}
		memset(entries,0,sizeof(char*)*numentries);
		memset(names,0,sizeof(char*)*numentries);

		child=item->child;depth++;
		//if(child->child)
		//	if (fmt) len+=2*depth-1;
		//else
			if (fmt) len+=depth;
		while (child)
		{
			names[i]=str=print_valuestring(child->name,0);
			entries[i++]=ret=print_value(child,depth,fmt,0);
			if (str && ret) len+=strlen(ret)+strlen(str)+2+(fmt?2+depth:0); else fail=1;
			child=child->next;
		}
		if (!fail)	out=(char*)json_alloc(len);
		if (!out) fail=1;

		if (fail)
		{
			for (i=0;i<numentries;i++) {if (names[i]) json_free(names[i]);if (entries[i]) json_free(entries[i]);}
			json_free(names);json_free(entries);
			return 0;
		}
		//if(item->child->child)
		//{	
		//	ptr=out;
		//	if(json_is_array(item->child) || json_is_object(item->child))				//add
		//		if (fmt) for (j=0;j<depth-1;j++) *ptr++='\t';							//add
		//	*ptr++='{';if (fmt)*ptr++='\n';*ptr=0;									//add
		//}else
		//{
			*out='{';ptr=out+1;if (fmt)*ptr++='\n';*ptr=0;
		//}
		for (i=0;i<numentries;i++)
		{
			if (fmt) for (j=0;j<depth;j++) *ptr++='\t';
			tmplen=strlen(names[i]);memcpy(ptr,names[i],tmplen);ptr+=tmplen;
			*ptr++=':';if (fmt) *ptr++='\t';
			//*ptr++=':';if (fmt) *ptr++='\n';
			strcpy(ptr,entries[i]);ptr+=strlen(entries[i]);
			if (i!=numentries-1) *ptr++=',';
			if (fmt) *ptr++='\n';*ptr=0;
			json_free(names[i]);json_free(entries[i]);
		}
		
		json_free(names);json_free(entries);
		if (fmt) for (i=0;i<depth-1;i++) *ptr++='\t';
		*ptr++='}';*ptr++=0;
	}
	return out;	
}

JSTATIC char *print_value(json_ht item,int depth,int fmt,json_buffer_ht p)
{
	char *out=NULL;
	if (!item) return NULL;
	if (p)
	{
		switch ((item->type)&0x7f)
		{
			case JSON_NULL:		{out=string_ensure(p,5); if (out) strcpy(out,"null"); break;}
			case JSON_FALSE:	{out=string_ensure(p,6); if (out) strcpy(out,"false");	break;}
			case JSON_TRUE:		{out=string_ensure(p,5); if (out) strcpy(out,"true"); break;}
			case JSON_NUMBER:	out=print_number(item,p);break;
			case JSON_STRING:	out=print_string(item,p);break;
			case JSON_ARRAY:	out=print_array(item,depth,fmt,p);break;
			case JSON_OBJECT:	out=print_object(item,depth,fmt,p);break;
		}
	}
	else
	{
		switch ((item->type)&0x7f)
		{
			case JSON_NULL:		out=json_strdup("null"); break;
			case JSON_FALSE:	out=json_strdup("false"); break;
			case JSON_TRUE:		out=json_strdup("true"); break;
			case JSON_NUMBER:	out=print_number(item,0); break;
			case JSON_STRING:	out=print_string(item,0); break;
			case JSON_ARRAY:	out=print_array(item,depth,fmt,0); break;
			case JSON_OBJECT:	out=print_object(item,depth,fmt,0); break;
		}
	}
	return out;
}

char *json_print(json_ht item,int fmt) {if(!fmt)return print_value(item,0,0,0);else return print_value(item,0,1,0);}
char *json_print_buffered(json_ht item,int size,int fmt)
{
	json_buffer_t p;
	p.buffer=(char*)json_alloc(size);
	p.length=size;
	p.offset=0;
	//return print_value(item,0,fmt,&p);
	print_value(item,0,fmt,&p);
	return p.buffer;
}

int json_array_size(json_ht array)								{json_ht c=array->child;int i=0;while(c)i++,c=c->next;return i;}
json_ht json_array_get(json_ht array,int item)				{json_ht c=array->child;  while (c && item>0) item--,c=c->next; return c;}
json_ht json_object_get(json_ht object,const char *string)	{json_ht c=object->child; while (c && json_strcasecmp(c->name,string)) c=c->next; return c;}

JSTATIC void suffix_object(json_ht prev,json_ht item) {prev->next=item;item->prev=prev;}

JSTATIC json_ht create_reference(json_ht item) {json_ht ref=json_new_item();if (!ref) return 0;memcpy(ref,item,sizeof(json_t));ref->name=0;ref->type|=JSON_IS_REFERENCE;ref->next=ref->prev=0;return ref;}

void json_array_add(json_ht array,json_ht item) {json_ht c=array->child;if (!item) return; if (!c) {array->child=item;} else {while (c && c->next) c=c->next; suffix_object(c,item);}}

void json_object_add(json_ht object,const char *string,json_ht item) {if (!item) return; if (item->name) json_free(item->name);item->name=json_strdup(string);json_array_add(object,item);}

//object的name为常量字符串，直接使用指针
void json_object_add_cs(json_ht object,const char *string,json_ht item) {if (!item) return; if (!(item->type&JSON_IS_STR_CONST) && item->name) json_free(item->name);item->name=(char*)string;item->type|=JSON_IS_STR_CONST;json_array_add(object,item);}

void json_array_reference_add(json_ht array,json_ht item) {json_array_add(array,create_reference(item));}

void json_object_reference_add(json_ht object,const char *string,json_ht item)	{json_object_add(object,string,create_reference(item));}

//将array中的一个item分离出来
json_ht json_array_detach(json_ht array,int which) {json_ht c=array->child;while (c && which>0) c=c->next,which--;if (!c) return 0;if (c->prev) c->prev->next=c->next;if (c->next) c->next->prev=c->prev;if (c==array->child) array->child=c->next;c->prev=c->next=0;return c;}

//将array中索引为which的删除
void json_array_del(json_ht array,int which) {json_delete(json_array_detach(array,which));}

json_ht json_object_detach(json_ht object,const char *string) {int i=0;json_ht c=object->child;while (c && json_strcasecmp(c->name,string)) i++,c=c->next;if (c) return json_array_detach(object,i);return NULL;}
void json_object_del(json_ht object,const char *string) {json_delete(json_object_detach(object,string));}

//array插入操作
void json_array_insert(json_ht array,int which,json_ht newitem) {json_ht c=array->child;while (c && which>0) c=c->next,which--;if (!c) {json_array_add(array,newitem);return;}newitem->next=c;newitem->prev=c->prev;c->prev=newitem;if (c==array->child) array->child=newitem; else newitem->prev->next=newitem;}

//替换object中的item
void json_array_replace(json_ht array,int which,json_ht newitem) {json_ht c=array->child;while (c && which>0) c=c->next,which--;if (!c) return;newitem->next=c->next;newitem->prev=c->prev;if (newitem->next) newitem->next->prev=newitem;if (c==array->child) array->child=newitem; else newitem->prev->next=newitem;c->next=c->prev=0;json_delete(c);}
void json_object_replace(json_ht object,const char *string,json_ht newitem){int i=0;json_ht c=object->child;while(c && json_strcasecmp(c->name,string))i++,c=c->next;if(c){newitem->name=json_strdup(string);json_array_replace(object,i,newitem);}}

json_ht json_null_new(void)						{json_ht item=json_new_item();if(item)item->type=JSON_NULL;return item;}
json_ht json_true_new(void)						{json_ht item=json_new_item();if(item)item->type=JSON_TRUE;return item;}
json_ht json_false_new(void)					{json_ht item=json_new_item();if(item)item->type=JSON_FALSE;return item;}
json_ht json_bool_new(int b)					{json_ht item=json_new_item();if(item)item->type=b?JSON_TRUE:JSON_FALSE;return item;}
json_ht json_number_new(double num)				{json_ht item=json_new_item();if(item){item->type=JSON_NUMBER;item->valuedouble=num;item->valueint=(int)num;}return item;}
json_ht json_string_new(const char *string)		{json_ht item=json_new_item();if(item){item->type=JSON_STRING;item->valuestring=json_strdup(string);}return item;}
json_ht json_array_new(void)					{json_ht item=json_new_item();if(item)item->type=JSON_ARRAY;return item;}
json_ht json_object_new(void)					{json_ht item=json_new_item();if(item)item->type=JSON_OBJECT;return item;}

json_ht json_array_int_new(const int *numbers,int count)			{int i;json_ht n=0,p=0,a=json_array_new();for(i=0;a && i<count;i++){n=json_number_new(numbers[i]);if(!i)a->child=n;else suffix_object(p,n);p=n;}return a;}
json_ht json_array_float_new(const float *numbers,int count)		{int i;json_ht n=0,p=0,a=json_array_new();for(i=0;a && i<count;i++){n=json_number_new(numbers[i]);if(!i)a->child=n;else suffix_object(p,n);p=n;}return a;}
json_ht json_array_double_new(const double *numbers,int count)		{int i;json_ht n=0,p=0,a=json_array_new();for(i=0;a && i<count;i++){n=json_number_new(numbers[i]);if(!i)a->child=n;else suffix_object(p,n);p=n;}return a;}
json_ht json_array_string_new(const char **strings,int count)		{int i;json_ht n=0,p=0,a=json_array_new();for(i=0;a && i<count;i++){n=json_string_new(strings[i]);if(!i)a->child=n;else suffix_object(p,n);p=n;}return a;}

json_ht json_duplicate(json_ht item,int recurse)
{
	json_ht newitem,cptr,nptr=0,newchild;
	if (!item) return 0;
	newitem=json_new_item();
	if (!newitem) return 0;
	newitem->type=item->type&(~JSON_IS_REFERENCE),newitem->valueint=item->valueint,newitem->valuedouble=item->valuedouble;
	if (item->valuestring) {newitem->valuestring=json_strdup(item->valuestring); if (!newitem->valuestring) {json_delete(newitem);return 0;}}
	if (item->name) {newitem->name=json_strdup(item->name); if (!newitem->name) {json_delete(newitem);return 0;}}
	if (!recurse) return newitem;
	cptr=item->child;
	while (cptr)
	{
		newchild=json_duplicate(cptr,1);
		if (!newchild) {json_delete(newitem);return 0;}
		if (nptr) {nptr->next=newchild,newchild->prev=nptr;nptr=newchild;}
		else {newitem->child=newchild;nptr=newchild;}
		cptr=cptr->next;
	}
	return newitem;
}

void json_minify(char *json)
{
	char *into=json;
	while (*json)
	{
		if (*json==' ') json++;
		else if (*json=='\t') json++;
		else if (*json=='\r') json++;
		else if (*json=='\n') json++;
		else if (*json=='/' && json[1]=='/')  while (*json && *json!='\n') json++;//单行注释
		else if (*json=='/' && json[1]=='*') {while (*json && !(*json=='*' && json[1]=='/')) json++;json+=2;} //多行注释
		else if (*json=='\"'){*into++=*json++;while (*json && *json!='\"'){if (*json=='\\') *into++=*json++;*into++=*json++;}*into++=*json++;} 
		else *into++=*json++;//其它字符
	}
	*into=0;//null终结
}

json_ht json_parse_file(char *filename)
{
	json_ht json=NULL;
	FILE *f;long len;char *data;	
	f=fopen(filename,"rb");fseek(f,0,SEEK_END);len=ftell(f);fseek(f,0,SEEK_SET);
	data=(char*)json_alloc(JALIGN(len+1,4));fread(data,1,len,f);fclose(f);
	json=json_parse(data);
	json_free(data);
	if (!json) {printf("Failed to parse file: %s, error:[%s]\n",filename,json_error_get());}
	return json;
}

int json_saveto_file(json_ht item,char *filename)
{
	FILE *f;long len;char *data=NULL;
	data = json_print(item,0);
	if(!data)return -1;
	f=fopen(filename,"wb+");
	if(!f)
	{
		json_free(data);
		return -1;
	}
	len = strlen(data);
	fwrite(data,1,len,f);fclose(f);json_free(data);
	return 0;
}












