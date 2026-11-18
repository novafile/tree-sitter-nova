#include <Python.h>

typedef struct TSLanguage TSLanguage;

TSLanguage *tree_sitter_nova(void);
TSLanguage *tree_sitter_nova_schema(void);
TSLanguage *tree_sitter_nova_lines(void);

static PyObject* _binding_language(PyObject *Py_UNUSED(self), PyObject *Py_UNUSED(args)) {
    return PyCapsule_New(tree_sitter_nova(), "tree_sitter.Language", NULL);
}

static PyObject* _binding_language_nova_schema(PyObject *Py_UNUSED(self), PyObject *Py_UNUSED(args)) {
    return PyCapsule_New(tree_sitter_nova_schema(), "tree_sitter.Language", NULL);
}

static PyObject* _binding_language_nova_lines(PyObject *Py_UNUSED(self), PyObject *Py_UNUSED(args)) {
    return PyCapsule_New(tree_sitter_nova_lines(), "tree_sitter.Language", NULL);
}

static struct PyModuleDef_Slot slots[] = {
#ifdef Py_GIL_DISABLED
    {Py_mod_gil, Py_MOD_GIL_NOT_USED},
#endif
    {0, NULL}
};

static PyMethodDef methods[] = {
    {"language", _binding_language, METH_NOARGS,
     "Get the tree-sitter language for NOVA text (.nova)."},
    {"language_nova_schema", _binding_language_nova_schema, METH_NOARGS,
     "Get the tree-sitter language for the NOVA schema language (.novs)."},
    {"language_nova_lines", _binding_language_nova_lines, METH_NOARGS,
     "Get the tree-sitter language for NOVA Lines (.novl)."},
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef module = {
    .m_base = PyModuleDef_HEAD_INIT,
    .m_name = "_binding",
    .m_doc = NULL,
    .m_size = 0,
    .m_methods = methods,
    .m_slots = slots,
};

PyMODINIT_FUNC PyInit__binding(void) {
    return PyModuleDef_Init(&module);
}
