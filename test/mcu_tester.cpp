//
//  mcu_tester.cpp
//  mcu_tester
//
//  Created by sunkw on 14-5-12.
//  Copyright (c) 2014年 sunkw. All rights reserved.
//

#include "mcu_tester.h"
#include "../mcu/gsoap/mcu.nsmap"

static void usage(const char *name);

struct StartArgs
{
    char *command;
    char *ip;
    int port;
    char *params;
};

static int do_command(StartArgs &args);

int main(int argc, char **argv)
{
    StartArgs args = { 0, strdup("127.0.0.1"), 10110, 0 };
    
    int opt = getopt(argc, argv, "c:s:p:o:?h");
    while (opt != -1) {
        switch (opt) {
            case 'c':
                args.command = strdup(optarg);
                break;
                
            case 's':
                args.ip = strdup(optarg);
                break;
                
            case 'p':
                args.port = atoi(optarg);
                break;
                
            case 'o':
                args.params = strdup(optarg);
                break;
                
            case '?':
            default:
                usage(argv[0]);
                break;
        }
    }
    
    if (!args.command) {
        fprintf(stderr, "ERR: command not supplied!!\n");
        
        usage(argv[0]);
        return -1;
    }
    
    return do_command(args);
}

typedef int (*pfn_command)(StartArgs &args);

static int func_create_conference(StartArgs &args);
static int func_destroy_conference(StartArgs &args);
static int func_add_member(StartArgs &args);
static int func_del_member(StartArgs &args);

struct NamedFunc
{
    const char *name;
    pfn_command func;
    const char *desc;
};

static struct NamedFunc _funcs[] = {
    { "create_conference", func_create_conference, "to create a conference, no params" },
    { "destroy_conference", func_destroy_conference, "to destroy conference, with <conf_id>" },
    { "add_member", func_add_member, "to add member for a conference, with <conf_id>" },
    { "del_member", func_del_member, "to del member for a conference, with <conf_id & member_id>" },
};

static void usage(const char *name)
{
    const char *usage = "-c <cmd> -s <mcu soap ip> -p <mcu soap port> -o <params>\n"
    "\t-c: command include:\n";
    
    fprintf(stderr, "usage: %s %s", name, usage);
    for (size_t i = 0; i < sizeof(_funcs) / sizeof(struct NamedFunc); i++) {
        fprintf(stderr, "\t\t%s: %s\n", _funcs[i].name, _funcs[i].desc);
    }

    const char *others =
    "\t-s: mcu gsoap endpoint ip, default 127.0.0.1\n"
    "\t-p: mcu gsoap endpoint port, default 10110\n"
    "\t-o: any params for command\n"
    "\n";
    fprintf(stderr, "%s", others);
}

static int do_command(StartArgs &args)
{
    for (size_t i = 0; i < sizeof(_funcs) / sizeof(struct NamedFunc); i++) {
        if (!strcmp(_funcs[i].name, args.command)) {
            return _funcs[i].func(args);
        }
    }
    
    return -1;
}

static int func_create_conference(StartArgs &args)
{
    return -1;
}

static int func_destroy_conference(StartArgs &args)
{
    return -1;
}

static int func_add_member(StartArgs &args)
{
    return -1;
}

static int func_del_member(StartArgs &args)
{
    return -1;
}
