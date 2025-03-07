## 1 [SurfaceFlinger](https://so.csdn.net/so/search?q=SurfaceFlinger&spm=1001.2101.3001.7020)渲染方案

[车载](https://so.csdn.net/so/search?q=%E8%BD%A6%E8%BD%BD&spm=1001.2101.3001.7020)的`倒车影像`同学们知道是怎么实现的吗？等待Android设备开机再打开倒车影像App？车子挂到R档倒车如果等待这个流程实在太久，是否还有其它办法可以让车子挂R档直接展示倒车影像呢？大家思考一下。  
我们从Android渲染的基础上思考，Android图像渲染到屏幕上通过`SurfaceFlinger`完成，如果我们不通过App的[Surface](https://so.csdn.net/so/search?q=Surface&spm=1001.2101.3001.7020)来完成对SurfaceFlinger的调用，是否可以直接通过某些方式拿到SurfaceFlinger把图像绘制在屏幕上？  
实际上，我们有如下两种方式：  
**1.通过SurfaceComposeClient拿到SurfaceFlinger，使用OpenGL直接渲染到屏幕上  
2.通过Hook拿到SurfaceFlinger，截获SurfaceFlinger的`eglSwapBuffers`方法调用替换为`new_eglSwapBuffers`方法。我们在`new_eglSwapBuffers`方法内调用原来的`eglSwapBuffers`并且绘制自定义的图像。**

下面我们来通过方式2实现自定义渲染：

## 2 Hook的方式实现渲染

### 2.1 实现思路

**1\. 实现一个Linux下的ptrace注入器: inject  
2\. 通过Hook注入的方式，将SurfaceFlinger渲染的方法拦截  
3\. 在拦截的方法内自定义渲染的逻辑**

### 2.2 Hook注入

Hook其实被应用的场景也比较多，比如平时我们使用AndroidStudio进行debug时，就是通过的这种Hook方式实现。需要对某个App的关键函数就行Hook，对于android而言，Smali层我们使用Xposed Hook框架，So层我们可以使用Cydia Substrate框架, Smali 层的Hook由于Xposed做的很好了， 但是关于Cydia Substrate对So的Hook使用起来很不好用，Cydia Substrate对So的Hook，会有以下问题：

1.  会所有android 应用程序的进行Hook
2.  很难实现对某个App 私有的so进行Hook
3.  Hook的事机 都是被动的，用户不能选择。  
    基于此，我们直接通过注入的方式实现Hook。

类似Windows 下的DLL 注入， 远程进程调用使用关键函数ptrace函数实现, 具体实现步骤如下：  
4\. 远程进程调用mmap 函数 在对方进程申请一块内存  
5\. 在这块内存中写上 so 的路径  
6\. 调用远程调用 dlopen 函数， dlopen就会调用so的所有void attribute((constructor)) 属性的函数  
7\. void attribute((constructor)) 属性的函数中实现我们的hook函数  
8\. hook 函数我们可以利用Cydia Substrat框架的MSFunctionHook函数量实现对某个函数进行Hook。

#### 2.2.1 ptrace注入器

之所以我们的注入程序叫做`ptrace注入器`，这是因为我们整个注入过程都在使用Linux的API -ptrace，ptrace是linux系统的一个编写调试器的api，此api必须在`root用户`才能调用，所以此种注入方式只能在root用户下使用，这里需要利用ptrace读取和修改寄存器的值和读写内存，以完成类似windows的dll注入功能。 下面的关键函数，已有大神帮我们封装的好了。  
Ptrace 注入器基本原理：

1.  attach目标进程，保存寄存器环境, 关键函数：ptrace\_attach , ptrace

```
    if (ptrace_attach(target_pid) == -1)
        goto exit;

    if (ptrace_getregs(target_pid, &regs) == -1)
        goto exit;
```

2.  远程进程调用 mmap在目标进程内申请一块内存, 关键函数：ptrace\_call\_wrapper

```
    mmap_addr = get_remote_addr(target_pid, libc_path, (void *)mmap);
    DEBUG_PRINT("[+] Remote mmap address: %x\n", mmap_addr);

    /* call mmap */
    parameters[0] = 0;                                  // addr
    parameters[1] = 0x8000;                             // size
    parameters[2] = PROT_READ | PROT_WRITE | PROT_EXEC; // prot
    parameters[3] = MAP_ANONYMOUS | MAP_PRIVATE;        // flags
    parameters[4] = 0;                                  //fd
    parameters[5] = 0;                                  //offset

    if (ptrace_call_wrapper(target_pid, "mmap", mmap_addr, parameters, 6, &regs) == -1)
        goto exit;

    map_base = ptrace_retval(&regs);
```

3.  把注入SO的路径写入上一步申请的地址空间， 关键函数：ptrace\_writedata

```
    ptrace_writedata(target_pid, map_base, library_path, strlen(library_path) + 1);
```

4.  远程进程调用函数dlopen，注意获取dlopen函数地址在linker模块中获取

```
// linker_path:/system/bin/linker
    dlopen_addr = get_remote_addr(target_pid, linker_path, (void *)dlopen);
    dlsym_addr = get_remote_addr(target_pid, linker_path, (void *)dlsym);
    dlclose_addr = get_remote_addr(target_pid, linker_path, (void *)dlclose);
    dlerror_addr = get_remote_addr(target_pid, linker_path, (void *)dlerror);

    DEBUG_PRINT("[+] Get imports: dlopen: %x, dlsym: %x, dlclose: %x, dlerror: %x\n",
                dlopen_addr, dlsym_addr, dlclose_addr, dlerror_addr);

    printf("library path = %s\n", library_path);
    ptrace_writedata(target_pid, map_base, library_path, strlen(library_path) + 1);

    parameters[0] = map_base;
    parameters[1] = RTLD_NOW | RTLD_GLOBAL;

    if (ptrace_call_wrapper(target_pid, "dlopen", dlopen_addr, parameters, 2, &regs) == -1)
        goto exit;

// ptrace_retval 获取远程进程调用的ret value
    void *sohandle = ptrace_retval(&regs);
```

5.  恢复环境，detach 目标进程: 关键函数：ptrace\_detach

```
    ptrace_setregs(target_pid, &original_regs);
    ptrace_detach(target_pid);
```

就此一个Linux 下的ptrace注入器就完成了。注: 由于笔者的代码是在android模拟上运行的， 代码是交叉编译，不便于调试，所以基本靠printf 完成代码的调试，但是这样效率比较低， 笔者建议看官朋友，可以在Linux系统完成这个注入程序的开发，这样不会笔者遇到一些问题，难以调试。 另外上面代码很多函数已经进行过封装了，具体项目地址看文章最后。  
另外：ptrace：还可以用于反调试ptrace (TRACEME)

### 2.3 SurfaceFlinger渲染

对于Android for arm上的`so注入（inject）`和`挂钩（hook）`，网上已有牛人给出了代码inject。由于实现中的ptrace函数是依赖于平台的，所以不经改动只能用于arm平台。本文将之扩展用于Android的`x86平台`。Arm平台部分基本重用了inject中的代码，其中因为汇编不好移植且容易出错，所以把shellcode.s用ptrace\_call替换掉了，另外保留了mmap，用来传字符串参数，当然也可以通过栈来传，但栈里和其它东西混一起，一弄不好就会隔儿了，所以还是保险点好。最后注意设备要root。还有就是要在Linux中配置一下NDK编译环境呀。  
首先创建目录及文件：  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/dd44150d288c4e22efb0e72248ca3ea3.png)

(说明：由于NDK编译的条件限制，所以我们需要创建的jni文件夹，当然这个文件的名字必须是`jni`)  
注入的核心源代码：

1.  inject.c

```
#include <stdio.h>
#include <stdlib.h>
#include <sys/user.h>
#include <asm/ptrace.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <dlfcn.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <elf.h>
#include <android/log.h>

#if defined(__i386__)
#define pt_regs user_regs_struct
#endif

#define ENABLE_DEBUG 1

#if ENABLE_DEBUG
#define LOG_TAG "INJECT"
#define LOGD(fmt, args...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, fmt, ##args)
#define DEBUG_PRINT(format, args...) \  
    LOGD(format, ##args)
#else
#define DEBUG_PRINT(format, args...)
#endif

#define CPSR_T_MASK (1u << 5)

const char *libc_path = "/system/lib/libc.so";
const char *linker_path = "/system/bin/linker";

int ptrace_readdata(pid_t pid, uint8_t *src, uint8_t *buf, size_t size)
{
    uint32_t i, j, remain;
    uint8_t *laddr;

    union u
    {
        long val;
        char chars[sizeof(long)];
    } d;

    j = size / 4;
    remain = size % 4;

    laddr = buf;

    for (i = 0; i < j; i++)
    {
        d.val = ptrace(PTRACE_PEEKTEXT, pid, src, 0);
        memcpy(laddr, d.chars, 4);
        src += 4;
        laddr += 4;
    }

    if (remain > 0)
    {
        d.val = ptrace(PTRACE_PEEKTEXT, pid, src, 0);
        memcpy(laddr, d.chars, remain);
    }

    return 0;
}

int ptrace_writedata(pid_t pid, uint8_t *dest, uint8_t *data, size_t size)
{
    uint32_t i, j, remain;
    uint8_t *laddr;

    union u
    {
        long val;
        char chars[sizeof(long)];
    } d;

    j = size / 4;
    remain = size % 4;

    laddr = data;

    for (i = 0; i < j; i++)
    {
        memcpy(d.chars, laddr, 4);
        ptrace(PTRACE_POKETEXT, pid, dest, d.val);

        dest += 4;
        laddr += 4;
    }

    if (remain > 0)
    {
        d.val = ptrace(PTRACE_PEEKTEXT, pid, dest, 0);
        for (i = 0; i < remain; i++)
        {
            d.chars[i] = *laddr++;
        }

        ptrace(PTRACE_POKETEXT, pid, dest, d.val);
    }

    return 0;
}

#if defined(__arm__)
int ptrace_call(pid_t pid, uint32_t addr, long *params, uint32_t num_params, struct pt_regs *regs)
{
    uint32_t i;
    for (i = 0; i < num_params && i < 4; i++)
    {
        regs->uregs[i] = params[i];
    }

    //
    // push remained params onto stack
    //
    if (i < num_params)
    {
        regs->ARM_sp -= (num_params - i) * sizeof(long);
        ptrace_writedata(pid, (void *)regs->ARM_sp, (uint8_t *)&params[i], (num_params - i) * sizeof(long));
    }

    regs->ARM_pc = addr;
    if (regs->ARM_pc & 1)
    {
        /* thumb */
        regs->ARM_pc &= (~1u);
        regs->ARM_cpsr |= CPSR_T_MASK;
    }
    else
    {
        /* arm */
        regs->ARM_cpsr &= ~CPSR_T_MASK;
    }

    regs->ARM_lr = 0;

    if (ptrace_setregs(pid, regs) == -1 || ptrace_continue(pid) == -1)
    {
        printf("error\n");
        return -1;
    }

    int stat = 0;
    waitpid(pid, &stat, WUNTRACED);
    while (stat != 0xb7f)
    {
        if (ptrace_continue(pid) == -1)
        {
            printf("error\n");
            return -1;
        }
        waitpid(pid, &stat, WUNTRACED);
    }

    return 0;
}

#elif defined(__i386__)
long ptrace_call(pid_t pid, uint32_t addr, long *params, uint32_t num_params, struct user_regs_struct *regs)
{
    regs->esp -= (num_params) * sizeof(long);
    ptrace_writedata(pid, (void *)regs->esp, (uint8_t *)params, (num_params) * sizeof(long));

    long tmp_addr = 0x00;
    regs->esp -= sizeof(long);
    ptrace_writedata(pid, regs->esp, (char *)&tmp_addr, sizeof(tmp_addr));

    regs->eip = addr;

    if (ptrace_setregs(pid, regs) == -1 || ptrace_continue(pid) == -1)
    {
        printf("error\n");
        return -1;
    }

    int stat = 0;
    waitpid(pid, &stat, WUNTRACED);
    while (stat != 0xb7f)
    {
        if (ptrace_continue(pid) == -1)
        {
            printf("error\n");
            return -1;
        }
        waitpid(pid, &stat, WUNTRACED);
    }

    return 0;
}
#else
#error "Not supported"
#endif

int ptrace_getregs(pid_t pid, struct pt_regs *regs)
{
    if (ptrace(PTRACE_GETREGS, pid, NULL, regs) < 0)
    {
        perror("ptrace_getregs: Can not get register values");
        return -1;
    }

    return 0;
}

int ptrace_setregs(pid_t pid, struct pt_regs *regs)
{
    if (ptrace(PTRACE_SETREGS, pid, NULL, regs) < 0)
    {
        perror("ptrace_setregs: Can not set register values");
        return -1;
    }

    return 0;
}

int ptrace_continue(pid_t pid)
{
    if (ptrace(PTRACE_CONT, pid, NULL, 0) < 0)
    {
        perror("ptrace_cont");
        return -1;
    }

    return 0;
}

int ptrace_attach(pid_t pid)
{
    if (ptrace(PTRACE_ATTACH, pid, NULL, 0) < 0)
    {
        perror("ptrace_attach");
        return -1;
    }

    int status = 0;
    waitpid(pid, &status, WUNTRACED);

    return 0;
}

int ptrace_detach(pid_t pid)
{
    if (ptrace(PTRACE_DETACH, pid, NULL, 0) < 0)
    {
        perror("ptrace_detach");
        return -1;
    }

    return 0;
}

void *get_module_base(pid_t pid, const char *module_name)
{
    FILE *fp;
    long addr = 0;
    char *pch;
    char filename[32];
    char line[1024];

    if (pid < 0)
    {
        /* self process */
        snprintf(filename, sizeof(filename), "/proc/self/maps", pid);
    }
    else
    {
        snprintf(filename, sizeof(filename), "/proc/%d/maps", pid);
    }

    fp = fopen(filename, "r");

    if (fp != NULL)
    {
        while (fgets(line, sizeof(line), fp))
        {
            if (strstr(line, module_name))
            {
                pch = strtok(line, "-");
                addr = strtoul(pch, NULL, 16);

                if (addr == 0x8000)
                    addr = 0;

                break;
            }
        }
        fclose(fp);
    }
    else
    {
        printf("get_module_base error!\n");
    }
    return (void *)addr;
}

void *get_remote_addr(pid_t target_pid, const char *module_name, void *local_addr)
{
    void *local_handle, *remote_handle;
    DEBUG_PRINT("[+] get_remote_addr:%s\n", module_name);

    local_handle = get_module_base(-1, module_name);
    remote_handle = get_module_base(target_pid, module_name);

    DEBUG_PRINT("[+] get_remote_addr: local[%x], remote[%x]\n", local_handle, remote_handle);

    void *ret_addr = (void *)((uint32_t)local_addr + (uint32_t)remote_handle - (uint32_t)local_handle);

    return ret_addr;
}

int find_pid_of(const char *process_name)
{
    int id;
    pid_t pid = -1;
    DIR *dir;
    FILE *fp;
    char filename[32];
    char cmdline[256];

    struct dirent *entry;

    if (process_name == NULL)
        return -1;

    dir = opendir("/proc");
    if (dir == NULL)
        return -1;

    while ((entry = readdir(dir)) != NULL)
    {
        id = atoi(entry->d_name);
        if (id != 0)
        {
            sprintf(filename, "/proc/%d/cmdline", id);
            fp = fopen(filename, "r");
            if (fp)
            {
                fgets(cmdline, sizeof(cmdline), fp);
                fclose(fp);

                if (strcmp(process_name, cmdline) == 0)
                {
                    /* process found */
                    pid = id;
                    break;
                }
            }
        }
    }

    closedir(dir);
    return pid;
}

long ptrace_retval(struct pt_regs *regs)
{
#if defined(__arm__)
    return regs->ARM_r0;
#elif defined(__i386__)
    return regs->eax;
#else
#error "Not supported"
#endif
}

long ptrace_ip(struct pt_regs *regs)
{
#if defined(__arm__)
    return regs->ARM_pc;
#elif defined(__i386__)
    return regs->eip;
#else
#error "Not supported"
#endif
}

int ptrace_call_wrapper(pid_t target_pid, const char *func_name, void *func_addr, long *parameters, int param_num, struct pt_regs *regs)
{
    DEBUG_PRINT("[+] Calling %s in target process.\n", func_name);
    if (ptrace_call(target_pid, (uint32_t)func_addr, parameters, param_num, regs) == -1)
        return -1;

    if (ptrace_getregs(target_pid, regs) == -1)
        return -1;
    DEBUG_PRINT("[+] Target process returned from %s, return value=%x, pc=%x \n",
                func_name, ptrace_retval(regs), ptrace_ip(regs));
    return 0;
}

int inject_remote_process(pid_t target_pid, const char *library_path, const char *function_name, const char *param, size_t param_size)
{
    int ret = -1;
    void *mmap_addr, *dlopen_addr, *dlsym_addr, *dlclose_addr, *dlerror_addr;
    void *local_handle, *remote_handle, *dlhandle;
    uint8_t *map_base = 0;
    uint8_t *dlopen_param1_ptr, *dlsym_param2_ptr, *saved_r0_pc_ptr, *inject_param_ptr, *remote_code_ptr, *local_code_ptr;

    struct pt_regs regs, original_regs;
    extern uint32_t _dlopen_addr_s, _dlopen_param1_s, _dlopen_param2_s, _dlsym_addr_s, \  
        _dlsym_param2_s,
        _dlclose_addr_s, _inject_start_s, _inject_end_s, _inject_function_param_s, \  
        _saved_cpsr_s,
        _saved_r0_pc_s;

    uint32_t code_length;
    long parameters[10];

    DEBUG_PRINT("[+] Injecting process: %d\n", target_pid);

    if (ptrace_attach(target_pid) == -1)
        goto exit;

    if (ptrace_getregs(target_pid, &regs) == -1)
        goto exit;

    /* save original registers */
    memcpy(&original_regs, &regs, sizeof(regs));

    mmap_addr = get_remote_addr(target_pid, libc_path, (void *)mmap);
    DEBUG_PRINT("[+] Remote mmap address: %x\n", mmap_addr);

    /* call mmap */
    parameters[0] = 0;                                  // addr
    parameters[1] = 0x8000;                             // size
    parameters[2] = PROT_READ | PROT_WRITE | PROT_EXEC; // prot
    parameters[3] = MAP_ANONYMOUS | MAP_PRIVATE;        // flags
    parameters[4] = 0;                                  //fd
    parameters[5] = 0;                                  //offset

    if (ptrace_call_wrapper(target_pid, "mmap", mmap_addr, parameters, 6, &regs) == -1)
        goto exit;

    map_base = ptrace_retval(&regs);

    dlopen_addr = get_remote_addr(target_pid, linker_path, (void *)dlopen);
    dlsym_addr = get_remote_addr(target_pid, linker_path, (void *)dlsym);
    dlclose_addr = get_remote_addr(target_pid, linker_path, (void *)dlclose);
    dlerror_addr = get_remote_addr(target_pid, linker_path, (void *)dlerror);

    DEBUG_PRINT("[+] Get imports: dlopen: %x, dlsym: %x, dlclose: %x, dlerror: %x\n",
                dlopen_addr, dlsym_addr, dlclose_addr, dlerror_addr);

    printf("library path = %s\n", library_path);
    ptrace_writedata(target_pid, map_base, library_path, strlen(library_path) + 1);

    parameters[0] = map_base;
    parameters[1] = RTLD_NOW | RTLD_GLOBAL;

    if (ptrace_call_wrapper(target_pid, "dlopen", dlopen_addr, parameters, 2, &regs) == -1)
        goto exit;

    void *sohandle = ptrace_retval(&regs);

#define FUNCTION_NAME_ADDR_OFFSET 0x100
    ptrace_writedata(target_pid, map_base + FUNCTION_NAME_ADDR_OFFSET, function_name, strlen(function_name) + 1);
    parameters[0] = sohandle;
    parameters[1] = map_base + FUNCTION_NAME_ADDR_OFFSET;

    if (ptrace_call_wrapper(target_pid, "dlsym", dlsym_addr, parameters, 2, &regs) == -1)
        goto exit;

    void *hook_entry_addr = ptrace_retval(&regs);
    DEBUG_PRINT("hook_entry_addr = %p\n", hook_entry_addr);

#define FUNCTION_PARAM_ADDR_OFFSET 0x200
    ptrace_writedata(target_pid, map_base + FUNCTION_PARAM_ADDR_OFFSET, param, strlen(param) + 1);
    parameters[0] = map_base + FUNCTION_PARAM_ADDR_OFFSET;

    if (ptrace_call_wrapper(target_pid, "hook_entry", hook_entry_addr, parameters, 1, &regs) == -1)
        goto exit;

    printf("hook success\n");
    // getchar();
    // parameters[0] = sohandle;

    // if (ptrace_call_wrapper(target_pid, "dlclose", dlclose, parameters, 1, &regs) == -1)
    //     goto exit;

    /* restore */
    ptrace_setregs(target_pid, &original_regs);
    ptrace_detach(target_pid);
    ret = 0;

exit:
    return ret;
}

int main(int argc, char **argv)
{
    pid_t target_pid;
    target_pid = find_pid_of("/system/bin/surfaceflinger");
    if (-1 == target_pid)
    {
        printf("Can't find the process\n");
        return -1;
    }
    printf("target_pid:%d\n", target_pid);
    DEBUG_PRINT("[+] Start\n");
    inject_remote_process(target_pid, "/data/local/tmp/libdraw.so", "hook_entry", "I'm parameter!", strlen("I'm parameter!"));
    return 0;
}
```

注意上面的/system/bin/surfaceflinger进程我随手写的，你的设备上不一定有。没有的话挑其它的也  
行，前提是ps命令里能找到。

2.  inject文件夹的Android.mk:

```
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := inject 
LOCAL_SRC_FILES := inject.c 
#shellcode.s
LOCAL_LDLIBS += -L$(SYSROOT)/usr/lib -llog
#LOCAL_FORCE_STATIC_EXECUTABLE := true
include $(BUILD_EXECUTABLE)
```

3.  draw.c

```
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <android/log.h>
#include <EGL/egl.h>
#include <GLES/gl.h>
#include <elf.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdbool.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <string.h>

#define LOG_TAG "INJECT"
#define LOGD(fmt, args...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, fmt, ##args)

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

#define LIBSF_PATH "/system/lib/libsurfaceflinger.so"
GLuint gProgram;
EGLBoolean (*old_eglSwapBuffers)(EGLDisplay dpy, EGLSurface surf) = -1;
EGLint w, h;
char *gVertexShader =
        "attribute vec4 vPosition;\n"
        "void main() {\n"
        "  gl_Position = vPosition;\n"
        "}\n";
//  字符串
char *gFragmentShader =
        "precision mediump float;\n"
        "void main() {\n"
        "  gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0);\n"
        "}\n";
// 这种代码运行 CPU  1   GPU 2  GPU 的代码       glsl 语法    字符语言  gpu  编译
void init_gl(){
//创建顶点程序
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);

    if (vertexShader) {
//         内存中  字符串   关联到CPU   开好的程序
        glShaderSource(vertexShader, 1, &gVertexShader, NULL);
//        GPU     编译
        glCompileShader(vertexShader);
//        检测编译成功
        GLint compiled = 0;
//        过程  java       对象.方法    面向过程= 面向状态
        glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &compiled);
//
        if (!compiled) {
            LOGD("创建片元程序失败");
            glDeleteShader(vertexShader);
            vertexShader = 0;
            return;
        }

        LOGD("创建顶点程序成功");

    }

//创建片元程序
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    if (fragmentShader) {
        glShaderSource(fragmentShader, 1, &gFragmentShader, NULL);
        glCompileShader(fragmentShader);

        GLint compiled = 0;
        glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &compiled);
        if (!compiled) {
            LOGD("创建片元程序失败");
            glDeleteShader(fragmentShader);
            fragmentShader = 0;
            return;
        }
        LOGD("创建片元程序成功");

    }
    gProgram = glCreateProgram();
    if (gProgram)
    {
        glAttachShader(gProgram, vertexShader);
        glAttachShader(gProgram, fragmentShader);
        glLinkProgram(gProgram);
        GLint linkStatus = GL_FALSE;
        glGetProgramiv(gProgram, GL_LINK_STATUS, &linkStatus);
        LOGD("开始创建总程序");
        if (linkStatus != GL_TRUE)
        {
            LOGD("总程序创建失败");
            glDeleteProgram(gProgram);
            gProgram = 0;
            return;
        }

    }

    LOGD("总程序创建成功");
//     程序  公司 连个部门

}

void drawRect(int x,int y,int width,int height){
    float  draw_x =0.1;
    float draw_y=0.1;
    float draw_width=0.3;
    float draw_height=0.3;
    const GLfloat Vertices[] = {
            draw_x,draw_y,0.0f,
            draw_x+draw_width,draw_y,0.0f,
            draw_x+draw_width,draw_y-draw_height,0.0f,
            draw_x,draw_y-draw_height,0.0f,
    };

//    绘制图像
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, Vertices);
    glDrawArrays(GL_LINE_LOOP, 0, 4);
}

//执行     我们从这里开始
//定义好了新函数  没事 1 s    简单操作  这一个
EGLBoolean new_eglSwapBuffers(EGLDisplay dpy, EGLSurface surface)
{
    LOGD("New eglSwapBuffers\n");
    eglQuerySurface(dpy, surface, EGL_WIDTH, &w);
    eglQuerySurface(dpy, surface, EGL_HEIGHT, &h);
    glUseProgram(gProgram);
    glLineWidth(2);
    glEnableVertexAttribArray(0);
    drawRect(0,0,200,200);

    return eglSwapBuffers(dpy, surface);
}


void *get_module_base(pid_t pid, const char *module_name)
{
    FILE *fp;
    long addr = 0;
    char *pch;
    char filename[32];
    char line[1024];

    if (pid < 0)
    {
        snprintf(filename, sizeof(filename), "/proc/self/maps", pid);
    }
    else
    {
        snprintf(filename, sizeof(filename), "/proc/%d/maps", pid);
    }

    fp = fopen(filename, "r");

    if (fp != NULL)
    {
        while (fgets(line, sizeof(line), fp))
        {
            if (strstr(line, module_name))
            {
                pch = strtok(line, "-");
                addr = strtoul(pch, NULL, 16);

                if (addr == 0x8000)
                    addr = 0;

                break;
            }
        }

        fclose(fp);
    }

    return (void *)addr;
}

#define LIBSF_PATH "/system/lib/libsurfaceflinger.so"
int hook_eglSwapBuffers()
{
//    系统函数
    old_eglSwapBuffers = eglSwapBuffers;
    LOGD("Orig eglSwapBuffers = %p\n", old_eglSwapBuffers);
//    找到这个地址eglSwapBuffers  的地址  map 工具 查找  surfaceflinger  找到这个地址eglSwapBuffers 的地址
    void *base_addr = get_module_base(getpid(), LIBSF_PATH);
    LOGD("libsurfaceflinger.so address = %p\n", base_addr);

    int fd;
    fd = open(LIBSF_PATH, O_RDONLY);
    if (-1 == fd)
    {
        LOGD("error\n");
        return -1;
    }

    Elf32_Ehdr ehdr;
    read(fd, &ehdr, sizeof(Elf32_Ehdr));

    unsigned long shdr_addr = ehdr.e_shoff;
    int shnum = ehdr.e_shnum;
    int shent_size = ehdr.e_shentsize;
    unsigned long stridx = ehdr.e_shstrndx;

    Elf32_Shdr shdr;
    lseek(fd, shdr_addr + stridx * shent_size, SEEK_SET);
    read(fd, &shdr, shent_size);

    char *string_table = (char *)malloc(shdr.sh_size);
    lseek(fd, shdr.sh_offset, SEEK_SET);
    read(fd, string_table, shdr.sh_size);
    lseek(fd, shdr_addr, SEEK_SET);

    int i;
    uint32_t out_addr = 0;
    uint32_t out_size = 0;
    uint32_t got_item = 0;
    int32_t got_found = 0;

    // LOGD("shnum=%d\n", shnum);
    for (int num = 0; num < shnum; num++)
    {
        read(fd, &shdr, shent_size);
        int name_idx = shdr.sh_name;
        if (strcmp(&(string_table[name_idx]), ".got.plt") == 0 || strcmp(&(string_table[name_idx]), ".got") == 0)
        {
            LOGD("%s\n", &(string_table[name_idx]));
            out_addr = base_addr + shdr.sh_addr;
            out_size = shdr.sh_size;
            LOGD("out_addr = %lx, out_size = %lx\n", out_addr, out_size);
            for (i = 0; i < out_size; i += 4)
            {
                got_item = *(uint32_t *)(out_addr + i);
                // LOGD("got_item = %lx\n", got_item);
                if (got_item == old_eglSwapBuffers)
                {
                    LOGD("Found eglSwapBuffers in got\n");
                    got_found = 1;

                    uint32_t page_size = getpagesize();
                    uint32_t entry_page_start = (out_addr + i) & (~(page_size - 1));
                    mprotect((uint32_t *)entry_page_start, page_size, PROT_READ | PROT_WRITE);
//                     新函数 注入这个地址
                    *(uint32_t *)(out_addr + i) = new_eglSwapBuffers;
//                    系统surfaceflinger 调用  eglSwapBuffers-----》new_eglSwapBuffers
                    break;
                }
                else if (got_item == new_eglSwapBuffers)
                {
                    LOGD("Already hooked\n");
                    break;
                }
            }
            if (got_found)
            {
                break;
            }
        }
    }
    free(string_table);
    close(fd);
}
int hook_entry(char *a)
{
//      了解 用    hook  设备其他设备，设备机型有关 小米 note 1 1
//正式开始  绘制  Opengl  C进程 渲染

    init_gl();
//    替换函数
//    函数 运行   目标进程  1     注入进程 2
    hook_eglSwapBuffers();
    return 0;
}
```

4.  yvan文件夹的Android.mk

```
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_CPPFLAGS += -std=c++11

LOCAL_LDLIBS += -L$(SYSROOT)/usr/lib -llog -lEGL -lGLESv1_CM -lGLESv2
#LOCAL_ARM_MODE := arm
LOCAL_MODULE    := draw
MY_CPP_LIST := $(wildcard $(LOCAL_PATH)/*.c)
LOCAL_SRC_FILES := $(MY_CPP_LIST:$(LOCAL_PATH)/%=%)

LOCAL_C_INCLUDES := \
$(LOCAL_PATH)/ \

include $(BUILD_SHARED_LIBRARY)
```

5.  项目的Android.mk

```
include $(call all-subdir-makefiles)
```

6.  项目的Application.mk:

```
APP_ABI := x86 armeabi-v7a
```

7.  CMakeLists.txt

```
# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

# Sets the minimum version of CMake required to build the native library.

cmake_minimum_required(VERSION 3.18.1)

# Declares and names the project.

project("nativehook")

# Creates and names a library, sets it as either STATIC
# or SHARED, and provides the relative paths to its source code.
# You can define multiple libraries, and CMake builds them for you.
# Gradle automatically packages shared libraries with your APK.

add_library( # Sets the name of the library.
        nativehook

        # Sets the library as a shared library.
        SHARED

        # Provides a relative path to your source file(s).
        native-lib.cpp
        jni/inject/inject.c
        jni/yvan/draw.c)

# Searches for a specified prebuilt library and stores the path as a
# variable. Because CMake includes system libraries in the search path by
# default, you only need to specify the name of the public NDK library
# you want to add. CMake verifies that the library exists before
# completing its build.

find_library( # Sets the name of the path variable.
        log-lib

        # Specifies the name of the NDK library that
        # you want CMake to locate.
        log)

# Specifies libraries CMake should link to your target library. You
# can link multiple libraries, such as libraries you define in this
# build script, prebuilt third-party libraries, or system libraries.

target_link_libraries( # Specifies the target library.
        nativehook

        # Links the target library to the log library
        # included in the NDK.
        ${log-lib})
```

9.  native-lib.cpp

```
#include <jni.h>
#include <string>

extern "C" JNIEXPORT jstring JNICALL
Java_com_example_nativehook_MainActivity_stringFromJNI(
        JNIEnv* env,
        jobject /* this */) {
    std::string draw = "Draw from C++";
    return env->NewStringUTF(draw.c_str());
}
```

### 2.4 编译可执行文件和注入的so

运行nkd-build编译成生x86平台下的可执行文件和注入的so：  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/08b308649e8672ed5ccfcf1d23c5e3e9.png)

然后就可以跑起来试试了，连接root过的Android设备或者打开模拟器。将inject和libdraw.so拷入设备，设执行权限，执行：先看看被注入进程（surfaceflinger）的mmap，可以看到我们的so已经被加载了，紧接着的那一块就是我们mmap出来的：  
从logcat中也可以看到so注入成功，并且以被注入进程的身份执行了so中的代码。将编译好的文件拷贝到设备的`data/local/tmp`目录(不一定是该目录)。

```
cd libs/x86
adb push inject /data/local/tmp
adb push libdraw.so /data/local/tmp
```

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/83bdf66d61a44bdbc91e26b928d711c8.png)  
然后`修改文件权限`，并`运行inject`，将libdraw.so注入到了此进程中。

```
adb shell
cd /data/local/tmp
chmod 777 inject
./inject
```

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/a29cfc00f7182197cf52c2b55fc7abb3.png)  
简单的注入成功，现在我们再来做一个实验，就是应用这套机制来截获`surfaceflinger`中的  
`eglSwapBuffers`调用，用替换为`new_eglSwapBuffers`方法。我们在`new_eglSwapBuffers`方法内调用原来的`eglSwapBuffers`并且绘制自定义的图像。