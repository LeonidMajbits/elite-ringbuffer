/* CPython buffer-export shim. No queue or OS protocol is reimplemented here.
 * A real exporter owns a Python lease pin; every Py_buffer holds this object.
 * Slices/casts retain CPython's managed buffer, not merely the first memoryview.
 * Private create() takes a trusted native address; it is not a sandbox boundary.
 */
#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#if defined(Py_GIL_DISABLED) && Py_GIL_DISABLED
#error "The release exporter requires a GIL-enabled CPython build"
#endif

typedef struct {
    PyObject_HEAD
    PyObject *owner;
    PyObject *cleanup_owner; /* Acyclic native-lifetime state, not user wrappers. */
    void *address;
    Py_ssize_t length;
    Py_ssize_t exports;
    pid_t pid;
    int readonly;
    int closed;
    int closing;
    int close_requested;
} EliteBuffer;

static int same_process(EliteBuffer *self)
{
    if (getpid() != self->pid) {
        PyErr_SetString(PyExc_RuntimeError, "inherited ELITE buffer: fork is not an attachment");
        return 0;
    }
    return 1;
}
static void rollback_export(EliteBuffer *self);
static int buffer_get(PyObject *object, Py_buffer *view, int flags)
{
    EliteBuffer *self=(EliteBuffer *)object;
    if (view==NULL) { PyErr_SetString(PyExc_BufferError,"NULL buffer request"); return -1; }
    view->obj=NULL;
    if (!same_process(self)) return -1;
    if (self->closed || self->close_requested || self->owner == NULL) {
        PyErr_SetString(PyExc_BufferError, "ELITE lease exporter is closed"); return -1;
    }
    if (self->exports == PY_SSIZE_T_MAX) {
        PyErr_SetString(PyExc_BufferError, "too many ELITE buffer exports"); return -1;
    }
    /* exports includes pending acquisitions BEFORE arbitrary Python executes.
     * close() must see this transaction even if _assert_live drops the GIL. */
    ++self->exports;
    PyObject *ok=PyObject_CallMethod(self->owner,"_assert_live",NULL);
    if (ok == NULL) { rollback_export(self); return -1; }
    Py_DECREF(ok);
    if (PyBuffer_FillInfo(view,object,self->address,self->length,self->readonly,flags)<0) {
        rollback_export(self);
        return -1;
    }
    return 0;
}
static int close_buffer(EliteBuffer *self)
{
    if (!same_process(self)) return -1;
    if (self->closing) {
        PyErr_SetString(PyExc_BufferError,"ELITE exporter cleanup is in progress");
        return -1;
    }
    if (self->closed) return 0;
    if (self->exports != 0) {
        PyErr_SetString(PyExc_BufferError,"live or pending buffer(s): release every view before transferring the slot");
        return -1;
    }
    self->closing=1;
    self->closed=1; /* Block reentrant acquisition throughout native cleanup. */
    PyObject *result=PyObject_CallMethod(self->cleanup_owner,"_drop_view_pin",NULL);
    if (result == NULL) { self->closed=0; self->closing=0; return -1; }
    Py_DECREF(result);
    self->address=NULL;
    self->closing=0;
    /* Close state is complete before decrefs execute further finalizers. */
    Py_CLEAR(self->owner);
    Py_CLEAR(self->cleanup_owner);
    return 0;
}
static void cleanup_unraisable(EliteBuffer *self)
{
    /* Preserve a failed native obligation outside the collectible wrapper graph.
     * Never expose a dying self to sys.unraisablehook (A09-07). */
    PyObject *et=NULL,*ev=NULL,*tb=NULL;
    PyErr_Fetch(&et,&ev,&tb);
    if (self->cleanup_owner != NULL) {
        PyObject *r=PyObject_CallMethod(self->cleanup_owner,"_exporter_cleanup_failed",NULL);
        if (r == NULL) PyErr_WriteUnraisable(Py_None);
        else Py_DECREF(r);
    }
    PyErr_Restore(et,ev,tb);
    PyErr_WriteUnraisable(Py_None);
}
/* An explicit finalization request can occur inside the validation callback.
 * A failing export must both unwind its count and honor a now-unblocked close,
 * while preserving the exception which caused the export to fail. */
static void rollback_export(EliteBuffer *self)
{
    --self->exports;
    if (self->exports==0 && self->close_requested && !self->closed && getpid()==self->pid) {
        PyObject *et=NULL,*ev=NULL,*tb=NULL;
        PyErr_Fetch(&et,&ev,&tb);
        if (close_buffer(self)<0) cleanup_unraisable(self);
        PyErr_Restore(et,ev,tb);
    }
}
static void buffer_release(PyObject *object, Py_buffer *view)
{
    EliteBuffer *self=(EliteBuffer *)object;
    (void)view;
    if (self->exports>0) --self->exports;
    if (self->exports==0 && self->close_requested && !self->closed && getpid()==self->pid) {
        PyObject *et=NULL,*ev=NULL,*tb=NULL;
        PyErr_Fetch(&et,&ev,&tb);
        if (close_buffer(self)<0) cleanup_unraisable(self);
        PyErr_Restore(et,ev,tb);
    }
}
static PyObject *buffer_close(EliteBuffer *self, PyObject *unused)
{
    (void)unused;
    if (close_buffer(self)<0) return NULL;
    Py_RETURN_NONE;
}
static PyObject *buffer_closed(EliteBuffer *self, void *unused)
{ (void)unused; return PyBool_FromLong(self->closed); }
static PyObject *buffer_exports(EliteBuffer *self, void *unused)
{ (void)unused; return PyLong_FromSsize_t(self->exports); }
static int buffer_traverse(EliteBuffer *self, visitproc visit, void *arg)
{
    Py_VISIT(self->owner);
    Py_VISIT(self->cleanup_owner);
    return 0;
}
static void buffer_finalize(PyObject *object)
{
    EliteBuffer *self=(EliteBuffer *)object;
    PyObject *et=NULL,*ev=NULL,*tb=NULL;
    PyErr_Fetch(&et,&ev,&tb);
    self->close_requested=1;
    /* A cyclic isolate can contain a memoryview. Do NOT revoke its Py_buffer.
     * Its release will finish cleanup; an externally rooted alias keeps the
     * entire exporter graph reachable and is not cleared by cyclic GC. */
    if (self->exports==0 && !self->closed && getpid()==self->pid && self->cleanup_owner!=NULL) {
        if (close_buffer(self)<0) cleanup_unraisable(self);
    }
    PyErr_Restore(et,ev,tb);
}
static int buffer_clear(EliteBuffer *self)
{
    self->close_requested=1;
    if (self->exports!=0) return 0;
    /* Finalization/release already attempted native cleanup. tp_clear only
     * severs Python edges and cannot run a second native ownership transfer. */
    self->closed=1;
    self->address=NULL;
    Py_CLEAR(self->owner);
    Py_CLEAR(self->cleanup_owner);
    return 0;
}
static void buffer_dealloc(EliteBuffer *self)
{
    /* CPython's helper detects resurrection during ANY finalizer callback. */
    if (PyObject_CallFinalizerFromDealloc((PyObject *)self)<0) return;
    PyObject_GC_UnTrack(self);
    (void)buffer_clear(self);
    Py_TYPE(self)->tp_free((PyObject *)self);
}
static PyBufferProcs buffer_procs={.bf_getbuffer=buffer_get,.bf_releasebuffer=buffer_release};
static PyMethodDef buffer_methods[]={
    {"close",(PyCFunction)buffer_close,METH_NOARGS,"Close only after the last exported view ends."},
    {NULL,NULL,0,NULL}
};
static PyGetSetDef buffer_getsets[]={
    {"closed",(getter)buffer_closed,NULL,"Whether new exports are forbidden.",NULL},
    {"exports",(getter)buffer_exports,NULL,"Outstanding buffer acquisitions (not slice count).",NULL},
    {NULL,NULL,NULL,NULL,NULL}
};
static PyTypeObject BufferType={
    PyVarObject_HEAD_INIT(NULL,0)
    .tp_name="_elite_buffer.LeaseBuffer",
    .tp_basicsize=sizeof(EliteBuffer),
    .tp_dealloc=(destructor)buffer_dealloc,
    .tp_flags=Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,
    .tp_traverse=(traverseproc)buffer_traverse,
    .tp_clear=(inquiry)buffer_clear,
    .tp_finalize=buffer_finalize,
    .tp_doc="Owner-retaining read-only/writable payload exporter; private factory only.",
    .tp_as_buffer=&buffer_procs,
    .tp_methods=buffer_methods,
    .tp_getset=buffer_getsets
};
static PyObject *module_create(PyObject *module, PyObject *args)
{
    (void)module;
    PyObject *pointer,*owner,*cleanup_owner=NULL; Py_ssize_t length; int readonly;
    if (!PyArg_ParseTuple(args,"OnpO|O:create",&pointer,&length,&readonly,&owner,&cleanup_owner)) return NULL;
    if (length<0) {PyErr_SetString(PyExc_ValueError,"negative buffer length");return NULL;}
    void *address=PyLong_AsVoidPtr(pointer);
    if (PyErr_Occurred()) return NULL;
    if (address==NULL) {PyErr_SetString(PyExc_ValueError,"NULL native payload");return NULL;}
    EliteBuffer *self=(EliteBuffer *)BufferType.tp_alloc(&BufferType,0);
    if (self==NULL) return NULL;
    self->owner=Py_NewRef(owner);
    self->cleanup_owner=Py_NewRef(cleanup_owner!=NULL?cleanup_owner:owner);
    self->address=address; self->length=length;
    self->readonly=readonly; self->closed=0; self->closing=0; self->close_requested=0; self->exports=0; self->pid=getpid();
    return (PyObject *)self;
}
/* Benchmark/example workload helpers, not transport primitives. Every byte
 * is generated or compared in the currently exported span; no Python payload
 * object or intermediate payload allocation is created. They intentionally
 * keep the GIL; separate processes run independently. */
static PyObject *pattern_operation(PyObject *args, int writing)
{
    PyObject *object,*id_object;
    if (!PyArg_ParseTuple(args,"OO",&object,&id_object)) return NULL;
    unsigned long long number=PyLong_AsUnsignedLongLong(id_object);
    if (PyErr_Occurred()) return NULL;
    Py_buffer buffer;
    if (PyObject_GetBuffer(object,&buffer,writing?PyBUF_WRITABLE:PyBUF_SIMPLE)<0) return NULL;
    unsigned char *bytes=buffer.buf;
    uint64_t id=(uint64_t)number;
    uint64_t pattern=id^UINT64_C(0xa5a5a5a5a5a5a5a5),mismatch=0;
    Py_ssize_t i=0;
    /* LE128 admission is little-endian. memcpy here emits unaligned-safe word
     * construction/checks from one scalar, not an intermediate payload copy. */
    for (; buffer.len-i>=8; i+=8) {
        if (writing) memcpy(bytes+i,&pattern,8);
        else {
            uint64_t observed;
            memcpy(&observed,bytes+i,8);
            mismatch |= observed^pattern;
        }
    }
    for (; i<buffer.len; ++i) {
        unsigned shift=(unsigned)(((size_t)i&7u)*8u);
        unsigned char expected=(unsigned char)(pattern>>shift);
        if (writing) bytes[i]=expected;
        else mismatch |= (uint64_t)(bytes[i]^expected);
    }
    PyBuffer_Release(&buffer);
    if (writing) Py_RETURN_NONE;
    return PyBool_FromLong(mismatch==0);
}
static PyObject *fill_pattern(PyObject *module,PyObject *args)
{ (void)module; return pattern_operation(args,1); }
static PyObject *verify_pattern(PyObject *module,PyObject *args)
{ (void)module; return pattern_operation(args,0); }
static PyMethodDef module_methods[]={
    {"fill_pattern",fill_pattern,METH_VARARGS,"Benchmark workload: construct every byte directly in a writable view."},
    {"verify_pattern",verify_pattern,METH_VARARGS,"Benchmark workload: compare every byte of a borrowed view."},
    {"create",module_create,METH_VARARGS,"Private: create an exporter over an already-pinned native lease."},
    {NULL,NULL,0,NULL}
};
static struct PyModuleDef module_definition={
    PyModuleDef_HEAD_INIT,
    .m_name="_elite_buffer",.m_doc="ELITE owner-retaining CPython buffer protocol adapter.",
    .m_size=-1,.m_methods=module_methods
};
PyMODINIT_FUNC PyInit__elite_buffer(void);
PyMODINIT_FUNC PyInit__elite_buffer(void)
{
    if (PyInterpreterState_GetID(PyThreadState_GetInterpreter(PyThreadState_Get()))!=0) {
        PyErr_SetString(PyExc_ImportError,"ELITE exporter supports the main interpreter only");return NULL;
    }
    if (PyType_Ready(&BufferType)<0) return NULL;
    PyObject *module=PyModule_Create(&module_definition);
    if (module==NULL) return NULL;
    if (PyModule_AddIntConstant(module,"API_VERSION",0x00010001)<0) {
        Py_DECREF(module);
        return NULL;
    }
    return module;
}
