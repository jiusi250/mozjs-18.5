
#ifndef DECOMPILER_MAIN_H
#define DECOMPILER_MAIN_H

#include "jsapi.h"
#include "jstl.h"
#include "jsvector.h"

class jsvalRoot {
public:
    explicit jsvalRoot(JSContext *context, jsval value = JSVAL_NULL)
            : cx(context), v(value) {
        if (!JS_AddValueRoot(cx, &v)) {
            fprintf(stderr, "Out of memory in jsvalRoot constructor, aborting\n");
            abort();
        }
    }

    ~jsvalRoot() { JS_RemoveValueRoot(cx, &v); }

    operator jsval() const { return value(); }

    jsvalRoot &operator=(jsval value) {
        v = value;
        return *this;
    }

    jsval *addr() { return &v; }

    jsval value() const { return v; }

private:
    JSContext *cx;
    jsval v;
};

/* Note: Aborts on OOM. */
class JSAPITestString {
    js::Vector<char, 0, js::SystemAllocPolicy> chars;
public:
    JSAPITestString() {}

    JSAPITestString(const char *s) { *this += s; }

    JSAPITestString(const JSAPITestString &s) { *this += s; }

    const char *begin() const { return chars.begin(); }

    const char *end() const { return chars.end(); }

    size_t length() const { return chars.length(); }

    JSAPITestString &operator+=(const char *s) {
        if (!chars.append(s, strlen(s)))
            abort();
        return *this;
    }

    JSAPITestString &operator+=(const JSAPITestString &s) {
        if (!chars.append(s.begin(), s.length()))
            abort();
        return *this;
    }
};

JSAPITestString toSource(JSContext *cx, jsval v) {
    JSString *str = JS_ValueToSource(cx, v);
    if (str) {
        JSAutoByteString bytes(cx, str);
        if (!!bytes)
            return JSAPITestString(bytes.ptr());
    }
    JS_ClearPendingException(cx);
    return JSAPITestString("<<error converting value to string>>");
}

bool fail(JSContext *cx, JSAPITestString msg = JSAPITestString(), const char *filename = "-", int lineno = 0) {
    if (JS_IsExceptionPending(cx)) {
        jsvalRoot v(cx);
        JS_GetPendingException(cx, v.addr());
        JS_ClearPendingException(cx);
        JSString *s = JS_ValueToString(cx, v);
        if (s) {
            JSAutoByteString bytes(cx, s);
            if (!!bytes)
                msg += bytes.ptr();
        }
    }
    fprintf(stderr, "%s:%d:%.*s\n", filename, lineno, (int) msg.length(), msg.begin());
    return false;
}

bool evaluate(JSContext *cx, JSObject *global, const char *bytes, const char *filename, int lineno, jsval *vp) {
    return JS_EvaluateScript(cx, global, bytes, strlen(bytes), filename, lineno, vp) ||
           fail(cx, bytes, filename, lineno);
}

#define EVAL(cx, global, s, vp) do { if (!evaluate(cx, global, s, __FILE__, __LINE__, vp)) return false; } while (false)

#define CHECK_SAME(cx, actual, expected) \
    do { \
        if (!checkSame(cx, actual, expected, #actual, #expected, __FILE__, __LINE__)) \
            return false; \
    } while (false)

bool checkSame(JSContext *cx, jsval actual, jsval expected,
               const char *actualExpr, const char *expectedExpr,
               const char *filename, int lineno) {
    JSBool same;

    JSAPITestString msg = JSAPITestString("CHECK_SAME failed: expected JS_SameValue(cx, ");
    msg += actualExpr;
    msg += ", ";
    msg += expectedExpr;
    msg += "), got !JS_SameValue(cx, ";
    msg += toSource(cx, actual);
    msg += ", ";
    msg += toSource(cx, expected);
    msg += ")";

    return (JS_SameValue(cx, actual, expected, &same) && same) ||
           fail(cx, msg, filename, lineno);
}

#define CHECK(cx, expr) \
    do { \
        if (!(expr)) \
            return fail(cx, "CHECK failed: " #expr, __FILE__, __LINE__); \
    } while (false)

#endif //DECOMPILER_MAIN_H
