#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include <iostream>
#include "ntwknode.h"

//using namespace std;

void parseIniFile(char *,struct NodeInfo *);
char * getKey(char *);
int getValue(char *);
void showNodeInfo(struct NodeInfo *);

