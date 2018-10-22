#include "main.h"

#include "jsapi.h"
#include "jsdbgapi.h"
#include "jsxdrapi.h"
#include "jsscript.h"

#include <iostream>

static JSObject *createGlobal(JSContext *cx) {

    static JSClass jsClass = {"global",
                              JSCLASS_GLOBAL_FLAGS,
                              JS_PropertyStub,
                              JS_PropertyStub,
                              JS_PropertyStub,
                              JS_StrictPropertyStub,
                              JS_EnumerateStub,
                              JS_ResolveStub,
                              JS_ConvertStub,
                              JS_FinalizeStub,
                              JSCLASS_NO_OPTIONAL_MEMBERS};

    /* Create the global object. */
    JSObject *global = JS_NewCompartmentAndGlobalObject(cx, &jsClass, NULL);
    if (!global)
        return NULL;

    JSAutoEnterCompartment ac;
    if (!ac.enter(cx, global))
        return NULL;

    /* Populate the global object with the standard globals,
       like Object and Array. */
    if (!JS_InitStandardClasses(cx, global))
        return NULL;
    return global;
}

bool test(JSContext *cx, JSObject *global) {
    const char *s =
            "function makeClosure(s, name, value) {\n"
            "    eval(s);\n"
            "    return let (n = name, v = value) function () { return String(v); };\n"
            "}\n"
            "var f = makeClosure('0;', 'status', 'ok');\n";

    // compile
    JSObject *scriptObj = JS_CompileScript(cx, global, s, strlen(s), __FILE__, __LINE__);
    CHECK(cx, scriptObj);

    // freeze
    JSXDRState *w = JS_XDRNewMem(cx, JSXDR_ENCODE);
    CHECK(cx, w);
    CHECK(cx, JS_XDRScriptObject(w, &scriptObj));
    uint32 nbytes;
    void *p = JS_XDRMemGetData(w, &nbytes);
    CHECK(cx, p);
    void *frozen = JS_malloc(cx, nbytes);
    CHECK(cx, frozen);
    memcpy(frozen, p, nbytes);
    JS_XDRDestroy(w);

    // thaw
    scriptObj = NULL;
    JSXDRState *r = JS_XDRNewMem(cx, JSXDR_DECODE);
    JS_XDRMemSetData(r, frozen, nbytes);
    CHECK(cx, JS_XDRScriptObject(r, &scriptObj));
    JS_XDRDestroy(r); // this frees `frozen`

    // execute
    jsvalRoot v2(cx);
    CHECK(cx, JS_ExecuteScript(cx, global, scriptObj, v2.addr()));

    // try to break the Block object that is the parent of f
    JS_GC(cx);

    // confirm
    EVAL(cx, global, "f() === 'ok';\n", v2.addr());
    jsvalRoot trueval(cx, JSVAL_TRUE);
    CHECK_SAME(cx, v2, trueval);

    JSString *decompiledScript = JS_DecompileScript(cx, scriptObj->getScript(), "hello", 1);
    char *encodedScriptBytes = JS_EncodeString(cx, decompiledScript);
    printf(encodedScriptBytes);

    return true;
}

int main() {
    // init runtime
    JSRuntime *rt = JS_NewRuntime(8L * 1024 * 1024);
    if (!rt) return 1;

    // init context
    JSContext *cx = JS_NewContext(rt, 8192);
    if (!cx) return 1;

    JS_SetOptions(cx, JSOPTION_VAROBJFIX | JSOPTION_JIT);
    JS_SetVersion(cx, JSVERSION_LATEST);

    // init global
    JSObject *global = createGlobal(cx);

    // run a sanity test
    test(cx, global);

    // clean up
    JS_DestroyContext(cx);
    JS_DestroyRuntime(rt);
    JS_ShutDown();
    return 0;
}
