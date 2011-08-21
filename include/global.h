/*
 * Copyright (C) 2010-2011 Beat Küng <beat-kueng@gmx.net>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef GLOBAL_H_
#define GLOBAL_H_


//stl
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <list>
#include <string>
#include <map>
#include <set>
#include <queue>
#include <stack>
#include <cmath>
using namespace std;

#include "config.h"
#include "exception.h"
#include "logging.h"


#define SAVE_DEL(x) if(x) { delete(x); x=NULL; }
#define SAVE_DEL_ARR(X) if(X) { delete[](X); X=NULL; }


string getDate(); //format: DD.MM.YY
string getTime(); //format: HH:MM:SS



/* useful string functions */

string toStr(int val);

string& toLower(string& str); //convert str to lower and return it
string toLower(const string& str);

bool cmpInsensitive(const string& str1, const string& str2);

string trim(const string& str);

string& replace(string& str, const string& find, const string& replace);



#endif /* GLOBAL_H_ */