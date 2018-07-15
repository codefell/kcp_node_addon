#include <node_api.h>
#include <string.h>
#include "ikcp.h"
#include <iostream>
#include <string>

using namespace std;

napi_env genv;

typedef struct tKcp {
    ikcpcb *kcp;
    napi_ref this_obj;
    napi_ref output_func;
} Kcp, *PKcp;

void log(napi_env env, const char *msg) {
    napi_value global;
    napi_value console;
    napi_value log_method;
    napi_value log_msg;
    napi_get_global(env, &global);
    napi_get_named_property(env, global, "console", &console);
    napi_get_named_property(env, console, "log", &log_method);
    napi_create_string_utf8(env, msg, NAPI_AUTO_LENGTH, &log_msg);
    napi_call_function(env, console, log_method, 1, &log_msg, NULL);
}

int udp_send(const char *buff, int len, ikcpcb *kcp, void *user)
{
    PKcp pkcp = (PKcp)user;
    napi_value array_buffer;
    napi_create_buffer_copy(genv, len, buff, NULL, &array_buffer);
    napi_value send_output, this_obj;
    napi_get_reference_value(genv, pkcp->output_func, &send_output);
    napi_get_reference_value(genv, pkcp->this_obj, &this_obj);
    napi_value result;
    napi_call_function(genv, this_obj, send_output, 1, &array_buffer, &result);
    return len;
}

PKcp _kcp_create(napi_env env, int32_t conv, napi_value this_obj, napi_value send_output) {
    PKcp pkcp = (PKcp)malloc(sizeof(Kcp));
    pkcp->kcp = ikcp_create(conv, (void *)pkcp);
    ikcp_nodelay(pkcp->kcp, 1, 10, 2, 1);
    napi_create_reference(env, send_output, 1, &pkcp->output_func);
    napi_create_reference(env, this_obj, 1, &pkcp->this_obj);
    ikcp_setoutput(pkcp->kcp, udp_send);
    return pkcp;
}

void kcp_release(napi_env env, void *data, void *hint)
{
    PKcp pkcp = (PKcp)data;
    if (pkcp!= NULL) {
        ikcp_release(pkcp->kcp);
        napi_reference_unref(env, pkcp->output_func, NULL);
        napi_reference_unref(env, pkcp->this_obj, NULL);
    }
    free(pkcp);
}

napi_value n_int_32(napi_env env, int32_t code)
{
    napi_value ncode;
    napi_create_int32(env, code, &ncode);
    return ncode;
}

napi_value kcp_create(napi_env env, napi_callback_info cbinfo)
{
    size_t argc = 2;
    napi_value argv[2];
    napi_value this_arg;
    int32_t conv;
    napi_get_cb_info(env, cbinfo, &argc, argv, &this_arg, NULL);
    napi_get_value_int32(env, argv[0], &conv);
    PKcp pkcp = _kcp_create(env, conv, this_arg, argv[1]);
    napi_value nkcp;
    napi_create_external(env, pkcp, kcp_release, NULL, &nkcp);
    string msg = "c++ conv = " + to_string(conv);
    log(env, msg.c_str());
    return nkcp;
}

napi_value kcp_update(napi_env env, napi_callback_info cbinfo)
{
    size_t argc = 2;
    napi_value argv[2];
    napi_value this_arg;
    napi_get_cb_info(env, cbinfo, &argc, argv, &this_arg, NULL);
    uint32_t millisec = 0;
    PKcp pkcp;
    napi_get_value_external(env, argv[0], (void **)&pkcp);
    napi_get_value_uint32(env, argv[1], &millisec);
    genv = env;
    ikcp_update(pkcp->kcp, millisec);
    return NULL;
}

napi_value kcp_input(napi_env env, napi_callback_info cbinfo)
{
    size_t argc = 2;
    napi_value argv[2];
    napi_value this_arg;
    napi_get_cb_info(env, cbinfo, &argc, argv, &this_arg, NULL);
    PKcp pkcp;
    napi_get_value_external(env, argv[0], (void **)&pkcp);
    void *data;
    size_t size;
    napi_get_buffer_info(env, argv[1], &data, &size);
    int is = ikcp_input(pkcp->kcp, (const char *)data, size);

    return NULL;
}

napi_value kcp_send(napi_env env, napi_callback_info cbinfo)
{
    size_t argc = 2;
    napi_value argv[2];
    napi_value this_arg;
    napi_get_cb_info(env, cbinfo, &argc, argv, &this_arg, NULL);
    PKcp pkcp;
    napi_get_value_external(env, argv[0], (void **)&pkcp);
    void *data;
    size_t size;
    napi_get_buffer_info(env, argv[1], (void **)&data, &size);
    ikcp_send(pkcp->kcp, (const char *)data, size);
    return NULL;
}

napi_value kcp_recv(napi_env env, napi_callback_info cbinfo)
{
    size_t argc = 1;
    napi_value argv[1];
    napi_value this_arg;
    napi_value ret = NULL;
    napi_get_cb_info(env, cbinfo, &argc, argv, &this_arg, NULL);
    PKcp pkcp;
    napi_get_value_external(env, argv[0], (void **)&pkcp);
    char buff[4096];
    napi_create_array(env, &ret);
    int i = 0;
    int size = 0;
    while ((size = ikcp_recv(pkcp->kcp, buff, 1024)) > 0) {
        napi_value recv_buff;
        napi_create_buffer_copy(env, size, buff, NULL, &recv_buff);
        napi_set_element(env, ret, i++, recv_buff);
    }
    return ret;
}

napi_value init(napi_env env, napi_value exports)
{
    struct _t {const char *method_name; napi_callback cb;} cbinfo[] = {
        {"kcp_create", kcp_create},
        {"kcp_update", kcp_update},
        {"kcp_input", kcp_input},
        {"kcp_send", kcp_send},
        {"kcp_recv", kcp_recv}
    };
    for (int i = 0; i < sizeof(cbinfo) / sizeof(cbinfo[0]); i++) {
        napi_value method;
        napi_create_function(env, NULL, 0, cbinfo[i].cb, NULL, &method);
        napi_set_named_property(env, exports, cbinfo[i].method_name, method);
    }
    return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, init)
