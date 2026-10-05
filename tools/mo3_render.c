#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
typedef struct openmpt_module openmpt_module;
openmpt_module* openmpt_module_create_from_memory2(const void*,size_t,void*,void*,void*,void*,int*,const char**,const void*);
double openmpt_module_set_position_order_row(openmpt_module*,int32_t,int32_t);
int32_t openmpt_module_get_current_order(openmpt_module*);
int32_t openmpt_module_get_current_row(openmpt_module*);
int openmpt_module_set_repeat_count(openmpt_module*,int32_t);
int openmpt_module_select_subsong(openmpt_module*,int32_t);
size_t openmpt_module_read_interleaved_stereo(openmpt_module*,int32_t,size_t,int16_t*);
int openmpt_module_set_render_param(openmpt_module*,int,int32_t);
int32_t openmpt_module_get_order_pattern(openmpt_module*,int32_t);
int main(int c,char**v){FILE*f=fopen(v[1],"rb");fseek(f,0,2);long n=ftell(f);rewind(f);void*b=malloc(n);fread(b,1,n,f);
 for(int a=3;a<c;a++){int start=strtol(v[a],0,16);
  openmpt_module*m=openmpt_module_create_from_memory2(b,n,0,0,0,0,0,0,0);
  openmpt_module_select_subsong(m,-1);
  openmpt_module_set_repeat_count(m,0);openmpt_module_set_render_param(m,3,8);
  openmpt_module_set_position_order_row(m,start,0);
  char fn[64];sprintf(fn,"%s_%02X.raw",v[2],start);FILE*o=fopen(fn,"wb");int16_t buf[1024*2];size_t k,tot=0;int prev=start,maxo=start,loopto=-1;
  while((k=openmpt_module_read_interleaved_stereo(m,44100,1024,buf))>0){
    int ord=openmpt_module_get_current_order(m);
    if(openmpt_module_get_order_pattern(m,ord)==199||ord<prev){loopto=ord;break;}
    if(ord>maxo)maxo=ord; prev=ord; fwrite(buf,4,k,o);tot+=k; if(tot>44100*400)break;}
  fclose(o);printf("%s start %02X last %02X loopto %02X %.1fs\n",fn,start,maxo,loopto,tot/44100.0);}
}
