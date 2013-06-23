/*
 * The contents of this file are copyright 2003 Jedediah Smith
 * <jedediah@silencegreys.com>
 * http://extension.ws/hlfix/
 *
 * This work is licensed under the Creative Commons "Attribution-Share Alike 3.0 Unported" License.
 * To view a copy of this license, visit http://creativecommons.org/licenses/by-sa/3.0/legalcode
 * or, send a letter to Creative Commons, 171 2nd Street, Suite 300, San Francisco, California, 94105, USA.
*/


#ifndef _INC_CD
#define _INC_CD

#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include "geo.h"

using namespace std;

void DecomposeGroup(GeoGroup *group);
void GenerateFaces(list<GeoEdge> *edges, GeoVector norm, list<GeoFace> *faces, const GeoTexture &tex);

#endif
