#ifndef USE_DJDEV64
#define USE_DJDEV64 1

struct pm_regs;
struct djdev64_ops {
    int (*open)(const char *path, unsigned flags);
    void (*close)(int handle);
    unsigned (*call)(int handle);
    unsigned (*ctrl)(int handle);
    unsigned (*stub)(void);
};

void register_djdev64(const struct djdev64_ops *ops);

#endif
