#include <dirent.h>
#include <fcntl.h>
#include <jni.h>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include <cstring>

#define NEO_UNUSED(x) x __attribute__((__unused__))
#ifdef __APPLE__
# define LACKS_PTSNAME_R
#endif

static int throw_runtime_exception(JNIEnv *env, const char *message) {
    jclass exClass = env->FindClass("java/lang/RuntimeException");
    if (!exClass) return -1; // Handle the case where FindClass fails.
    env->ThrowNew(exClass, message);
    return -1;
}

static int create_subprocess(JNIEnv *env,
                             char const *cmd,
                             char const *cwd,
                             char *const argv[],
                             char **envp,
                             int *pProcessId,
                             jint rows,
                             jint columns,
                             int cellWidth,
                             int cellHeight) {
    int ptm = open("/dev/ptmx", O_RDWR | O_CLOEXEC);
    if (ptm < 0) return throw_runtime_exception(env, "Cannot open /dev/ptmx");

#ifdef LACKS_PTSNAME_R
    char* devname;
#else
    char devname[64];
#endif
    if (grantpt(ptm) || unlockpt(ptm) ||
        #ifdef LACKS_PTSNAME_R
        (devname = ptsname(ptm)) == NULL
        #else
        ptsname_r(ptm, devname, sizeof(devname))
#endif
            ) {
        return throw_runtime_exception(env, "Cannot grantpt()/unlockpt()/ptsname_r() on /dev/ptmx");
    }

    // Enable UTF-8 mode and disable flow control to prevent Ctrl+S from locking up the display.
    struct termios tios;
    tcgetattr(ptm, &tios);
    tios.c_iflag |= IUTF8;
    tios.c_iflag &= ~(IXON | IXOFF);
    tcsetattr(ptm, TCSANOW, &tios);

    /** Set initial winsize. */
    struct winsize sz = {.ws_row = (unsigned short)(rows), .ws_col = (unsigned short) columns, .ws_xpixel = (unsigned short) (columns * cellWidth), .ws_ypixel = (unsigned short) (rows * cellHeight)};
    ioctl(ptm, TIOCSWINSZ, &sz);

    pid_t pid = fork();
    if (pid < 0) {
        return throw_runtime_exception(env, "Fork failed");
    } else if (pid > 0) {
        *pProcessId = (int) pid;
        return ptm;
    } else {
        // Clear signals which the Android java process may have blocked:
        sigset_t signals_to_unblock;
        sigfillset(&signals_to_unblock);
        sigprocmask(SIG_UNBLOCK, &signals_to_unblock, 0);

        close(ptm);
        setsid();

        int pts = open(devname, O_RDWR);
        if (pts < 0) exit(-1);

        dup2(pts, 0);
        dup2(pts, 1);
        dup2(pts, 2);

        DIR *self_dir = opendir("/proc/self/fd");
        if (self_dir != NULL) {
            int self_dir_fd = dirfd(self_dir);
            struct dirent *entry;
            while ((entry = readdir(self_dir)) != NULL) {
                int fd = atoi(entry->d_name);
                if (fd > 2 && fd != self_dir_fd) close(fd);
            }
            closedir(self_dir);
        }

        clearenv();
        if (envp) for (; *envp; ++envp) putenv(*envp);

        if (chdir(cwd) != 0) {
            char *error_message;
            // No need to free asprintf()-allocated memory since doing execvp() or exit() below.
            if (asprintf(&error_message, "chdir(\"%s\")", cwd) == -1)
                error_message = const_cast<char *>("chdir()");
            perror(error_message);
            fflush(stderr);
        }
        execvp(cmd, argv);
        // Show terminal output about failing exec() call:
        char *error_message;
        if (asprintf(&error_message, "exec(\"%s\")", cmd) == -1)
            error_message = const_cast<char *>("exec()");;
        perror(error_message);
        _exit(1);
    }
}

extern "C" JNIEXPORT jint JNICALL Java_com_offsec_nhterm_backend_JNI_createSubprocess(
        JNIEnv *env,
        jclass NEO_UNUSED(clazz),
        jstring cmd,
        jstring cwd,
        jobjectArray args,
        jobjectArray envVars,
        jintArray processIdArray,
        jint rows,
        jint columns,
        jint cellWidth,
        jint cellHeight) {
    jsize size = args ? env->GetArrayLength(args) : 0;
    char **argv = NULL;
    if (size > 0) {
        argv = (char **) malloc((size + 1) * sizeof(char *));
        if (!argv) return throw_runtime_exception(env, "Couldn't allocate argv array");
        for (int i = 0; i < size; ++i) {
            auto arg_java_string = (jstring) env->GetObjectArrayElement(args, i);
            char const *arg_utf8 = env->GetStringUTFChars(arg_java_string, nullptr);
            if (!arg_utf8)
                return throw_runtime_exception(env, "GetStringUTFChars() failed for argv");
            argv[i] = strdup(arg_utf8);
            env->ReleaseStringUTFChars(arg_java_string, arg_utf8);
        }
        argv[size] = NULL;
    }

    size = envVars ? env->GetArrayLength(envVars) : 0;
    char **envp = NULL;
    if (size > 0) {
        envp = (char **) malloc((size + 1) * sizeof(char *));
        if (!envp) return throw_runtime_exception(env, "malloc() for envp array failed");
        for (int i = 0; i < size; ++i) {
            jstring env_java_string = (jstring) env->GetObjectArrayElement(envVars, i);
            char const *env_utf8 = env->GetStringUTFChars(env_java_string, 0);
            if (!env_utf8)
                return throw_runtime_exception(env, "GetStringUTFChars() failed for env");
            envp[i] = strdup(env_utf8);
            env->ReleaseStringUTFChars(env_java_string, env_utf8);
        }
        envp[size] = NULL;
    }

    int procId = 0;
    char const *cmd_cwd = env->GetStringUTFChars(cwd, NULL);
    char const *cmd_utf8 = env->GetStringUTFChars(cmd, NULL);
    int ptm = create_subprocess(env, cmd_utf8, cmd_cwd, argv, envp, &procId, rows, columns, cellWidth, cellHeight);
    env->ReleaseStringUTFChars(cmd, cmd_utf8);
    env->ReleaseStringUTFChars(cmd, cmd_cwd);

    if (argv) {
        for (char **tmp = argv; *tmp; ++tmp) free(*tmp);
        free(argv);
    }
    if (envp) {
        for (char **tmp = envp; *tmp; ++tmp) free(*tmp);
        free(envp);
    }

    int *pProcId = (int *) env->GetPrimitiveArrayCritical(processIdArray, NULL);
    if (!pProcId)
        return throw_runtime_exception(env,
                                       "JNI call GetPrimitiveArrayCritical(processIdArray, &isCopy) failed");

    *pProcId = procId;
    env->ReleasePrimitiveArrayCritical(processIdArray, pProcId, 0);

    return ptm;
}

extern "C" JNIEXPORT void JNICALL
Java_com_offsec_nhterm_backend_JNI_setPtyWindowSize(JNIEnv *NEO_UNUSED(env),
                                              jclass NEO_UNUSED(clazz),
                                              jint fd,
                                              jint rows,
                                              jint cols,
                                              jint cell_width,
                                              jint cell_height) {
    struct winsize sz = {.ws_row = (unsigned short) rows, .ws_col = (unsigned short) cols, .ws_xpixel = (unsigned short) (cols * cell_width), .ws_ypixel = (unsigned short) (rows * cell_height)};
    ioctl(fd, TIOCSWINSZ, &sz);
}

extern "C" JNIEXPORT void JNICALL
Java_com_offsec_nhterm_backend_JNI_setPtyUTF8Mode(JNIEnv *NEO_UNUSED(env), jclass NEO_UNUSED(clazz),
                                            jint fd) {
    struct termios tios;
    tcgetattr(fd, &tios);
    if ((tios.c_iflag & IUTF8) == 0) {
        tios.c_iflag |= IUTF8;
        tcsetattr(fd, TCSANOW, &tios);
    }
}

extern "C" JNIEXPORT int JNICALL
Java_com_offsec_nhterm_backend_JNI_waitFor(JNIEnv *NEO_UNUSED(env), jclass NEO_UNUSED(clazz),
                                     jint pid) {
    int status;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        return -WTERMSIG(status);
    } else {
        // Should never happen - waitpid(2) says "One of the first three macros will evaluate to a non-zero (true) value".
        return 0;
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_offsec_nhterm_backend_JNI_close(JNIEnv *NEO_UNUSED(env), jclass NEO_UNUSED(clazz),
                                   jint fileDescriptor) {
    close(fileDescriptor);
}
