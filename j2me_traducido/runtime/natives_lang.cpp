// Native methods of java.lang / java.io / javax.microedition.rms.
#include "classes.h"
#include "platform.h"
#include "port.h"

#include <cstdio>

JObject* M_java_lang_Object__getClass___Ljava_lang_Class_(JObject* self) {
    return jclass_object(JNN(self)->cls);
}

int32_t M_java_lang_Object__hashCode___I(JObject* self) {
    return (int32_t)(((uintptr_t)self >> 3) * 2654435761u);
}

void M_java_io_PrintStream__write__Ljava_lang_String_Z_V(JObject* s, int32_t newline) {
    char buf[1024];
    jstring_to_utf8(s, buf, sizeof buf);
    printf("%s%s", buf, newline ? "\n" : "");
}

JObject* M_java_lang_Class__getName___Ljava_lang_String_(JObject* self) {
    JClass* c = (JClass*)(intptr_t)((J_java_lang_Class*)JNN(self))->f_vmClass_J;
    char buf[256];
    size_t i = 0;
    for (; c->name[i] && i < sizeof buf - 1; i++) buf[i] = c->name[i] == '/' ? '.' : c->name[i];
    buf[i] = 0;
    return jstring_from_utf8(buf);
}

JObject* M_java_lang_Class__loadResource__Ljava_lang_String__AB(JObject* name) {
    char buf[256];
    jstring_to_utf8(name, buf, sizeof buf);
    size_t size;
    const uint8_t* d = resource_find(buf, &size);
    if (d == nullptr) return nullptr;
    JObject* a = jnewarray(&AC_AB, (int32_t)size);
    memcpy(jadata<uint8_t>(a), d, size);
    return a;
}

int64_t M_java_lang_Runtime__freeMemory___J(JObject*) {
    size_t used = gc_heap_bytes();
    const size_t total = 16 * 1024 * 1024;
    return used < total ? (int64_t)(total - used) : 0;
}

int64_t M_java_lang_Runtime__totalMemory___J(JObject*) { return 16 * 1024 * 1024; }

void M_java_lang_Runtime__exit__I_V(JObject*, int32_t) {
    S_javax_microedition_midlet_MIDlet__exitRequested_Z = 1;
}

static int elem_size(JClass* c) {
    switch (c->elem_kind) {
        case 'J': case 'D': return 8;
        case 'I': case 'F': return 4;
        case 'C': case 'S': return 2;
        case 'B': case 'Z': return 1;
        default: return sizeof(JObject*);
    }
}

void M_java_lang_System__arraycopy__Ljava_lang_Object_ILjava_lang_Object_II_V(
    JObject* src, int32_t sp, JObject* dst, int32_t dp, int32_t len) {
    JNN(src);
    JNN(dst);
    JClass* sc = src->cls;
    JClass* dc = dst->cls;
    if (!(sc->flags & JCLS_ARRAY) || !(dc->flags & JCLS_ARRAY) ||
        (sc->elem_kind != dc->elem_kind && !(sc->elem_kind == 'L' && dc->elem_kind == 'L'))) {
        JObject* e = jnew(&C_java_lang_ArrayStoreException);
        M_java_lang_ArrayStoreException___init____V(e);
        jthrow(e);
    }
    int32_t sl = ((JArray*)src)->length, dl = ((JArray*)dst)->length;
    if (len < 0 || sp < 0 || dp < 0 || (int64_t)sp + len > sl || (int64_t)dp + len > dl) jthrow_aioobe(sp);
    int es = elem_size(sc);
    memmove(jadata<uint8_t>(dst) + (size_t)dp * es, jadata<uint8_t>(src) + (size_t)sp * es, (size_t)len * es);
}

int64_t M_java_lang_System__currentTimeMillis___J() { return platform_time_ms(); }

void M_java_lang_System__gc___V() { gc_request(); }

void M_java_lang_Thread__sleep__J_V(int64_t ms) {
    if (ms > 0) port_sleep((int)(ms > 1000 ? 1000 : ms));
}

// ---- RecordStore persistence ----

JObject* M_javax_microedition_rms_RecordStore__readFile__Ljava_lang_String__AB(JObject* name) {
    char buf[128];
    jstring_to_utf8(name, buf, sizeof buf);
    std::vector<uint8_t> data;
    if (!platform_load_save(buf, data)) return nullptr;
    JObject* a = jnewarray(&AC_AB, (int32_t)data.size());
    if (!data.empty()) memcpy(jadata<uint8_t>(a), data.data(), data.size());
    return a;
}

void M_javax_microedition_rms_RecordStore__writeFile__Ljava_lang_String_AB_V(JObject* name, JObject* data) {
    char buf[128];
    jstring_to_utf8(name, buf, sizeof buf);
    platform_store_save(buf, jadata<uint8_t>(JNN(data)), ((JArray*)data)->length);
}

void M_javax_microedition_rms_RecordStore__deleteFile__Ljava_lang_String__V(JObject* name) {
    char buf[128];
    jstring_to_utf8(name, buf, sizeof buf);
    platform_delete_save(buf);
}
