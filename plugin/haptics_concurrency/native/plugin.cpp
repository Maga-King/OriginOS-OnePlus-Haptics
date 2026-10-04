// SPDX-License-Identifier: Apache-2.0
#include "nyako_simple_plugin.h"
extern "C" const unsigned char concurrency_dex[],concurrency_dex_end[];
int nyako_plugin_main(const char* process){
    if(!nyako_is_system_server(process))return 1;
    return nyako_load_java_module(concurrency_dex,concurrency_dex_end-concurrency_dex,
        "haptics_concurrency",reinterpret_cast<const void*>(&nyako_plugin_main),
        "com.nyako.haptics.Entry",nullptr,0);
}
NYAKO_PLUGIN_REGISTER()
NYAKO_EXPORT int nyako_plugin_unload(){return -1;}
NYAKO_PLUGIN_DEFINE("Nyako 短触感并发",2,"android","通知期间补发短触感；配合并发版 HAL，完整重启生效")
