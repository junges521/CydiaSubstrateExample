#include <jni.h>
#include "substrate.h"
#include <android/log.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#define TAG "CYDIASUBSTRATE_HOOKTEST"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

MSConfig(MSFilterLibrary, "libfoo.so");
MSConfig(MSFilterLibrary, "libc.so");
MSConfig(MSFilterLibrary, "libdvm.so");

void performHook(const char *targetSo, const char *symbol, void *replace, void **original);

const int (*original_testPassword)(const char *myString);
const int faked_testPassword(const char *myString)
{
    LOGE("testPassword() is hooked!!!!!!, will always return ZERO !!!!!!");

    return 0;
}

static int (*original_strncmp)(const char*, const char*, size_t);
int faked_strncmp(const char* a, const char * b, size_t n)
{
    LOGE("strncmp() is hooked!!!!!!");
    if(strcmp("apple", a) == 0) {
        LOGE("strncmp() is hooked!!!!!!, will always return ZERO is the string is \"apple\" !!!!!!");
        return 0;
    }

    return original_strncmp(a, b, n);
}

static pid_t (*original_fork)();
pid_t faked_fork() {
    LOGE("fork() is hooked!!!!!!");
    if(fork()) {
        LOGE("fork() is hooked!!!!!!");
        return fork();
    }

    return original_fork();
}

void* find_symbol(const char* libraryname, const char* symbolname)
{
    void *imagehandle = dlopen(libraryname, RTLD_GLOBAL | RTLD_NOW);
    if (imagehandle != NULL) {
        void * sym = dlsym(imagehandle, symbolname);
        if (sym != NULL) {
            LOGE("symbol (%s) is found at address %p (%p) in lib %s", symbolname, sym, &sym, libraryname);
            return sym;
        } else {
            LOGE("find_symbol() can't find symblo (%s).", symbolname);
            return NULL;
        }
    } else {
        LOGE("dlopen error: %s, when opening lib %s",dlerror(), libraryname);
        return NULL;
    }
}

// Substrate entry point
MSInitialize{

    // Hook testPassword from libfoo
    const char *fooSoPath = "/data/data/com.example.hooktest.sampleapp/lib/libfoo.so";
    performHook(fooSoPath, "testPassword", (void*)&faked_testPassword, (void**)&original_testPassword);

    // Hook system libc library. Be careful, it might cause system to hang and need to reset.

    // Hook strncmp
    performHook("/system/lib/libc.so", "strncmp", (void*)&faked_strncmp, (void**)&original_strncmp);

    // Hook fork
    performHook("/system/lib/libc.so", "fork", (void*)&faked_fork, (void**)&original_fork);

}

void performHook(const char *targetSoPath, const char *symbol, void *replace, void **original) {

    // MSGetImageByName only work when the .so has already been loaded.
    MSImageRef image = MSGetImageByName(targetSoPath);

    void *symAddress;

    if (image != NULL) {
        LOGE("===>>>>>> MSGetImageByName (%s) succeed, symbol address: %p", targetSoPath, image);
        symAddress = MSFindSymbol(image, symbol);
        LOGE("===>>>>>> MSFindSymbol (%s) succeed, symbol address: %p", symbol, symAddress);
        MSHookFunction(symAddress, replace, original);
    } else { // When the .so has not been loaded, need to use below code to find the symbol.
        LOGE("Image not found, trying with find_symbol");
        // The following will work, as it will forcefully load given library.
        symAddress = find_symbol(targetSoPath, symbol);
        if (symAddress != NULL) {
            LOGE("===>>>>>> find_symbol (%s) succeed, symbol address: %p", symbol, symAddress);
            MSHookFunction(symAddress, replace, original);
            LOGE("===>>>>>> find_symbol (%s) succeed, symbol address: %p", symbol, symAddress);
        } else {
            LOGE("===>>>>>> find_symbol failed");
        }
    }
}