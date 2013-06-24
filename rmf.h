/*
 * The contents of this file are copyright 2003 Jedediah Smith
 * <jedediah@silencegreys.com>
 * http://extension.ws/hlfix/
 *
 * This work is licensed under the Creative Commons "Attribution-Share Alike 3.0 Unported" License.
 * To view a copy of this license, visit http://creativecommons.org/licenses/by-sa/3.0/legalcode
 * or, send a letter to Creative Commons, 171 2nd Street, Suite 300, San Francisco, California, 94105, USA.
*/

#ifndef _INC_RMF
#define _INC_RMF

#include <list>
#include <vector>
#include <stdio.h>

using namespace std;

extern int RMFPos;
extern int RMFPosVector;
extern int RMFPosSolid;
extern int RMFPosFace;
extern int RMFPosEntity;
extern int RMFPosGroup;
extern int RMFPosKey;
extern int RMFPosVisGroup;
extern int RMFPosPath;
extern int RMFPosCorner;

class RMFException
{
  public:
  char *msg;
  RMFException(char *toMsg) {msg = toMsg;}
};

class RMFColor
{
  public:
  unsigned char r, g, b;
};

class RMFVector
{
  public:
  float x, y, z;
};

class RMFFace
{
  public:
  char texture[256];
  RMFVector uaxis, vaxis;
  float ushift, vshift, uscale, vscale, rot;
  int nverts;
  list<list<RMFVector>::iterator> verts;
  vector<list<RMFVector>::iterator> planeverts;
};

class RMFKey
{
  public:
  char name[32];
  char value[100];
};

class RMFObject
{
  public:
  int visgroup;
  RMFColor color;
};

class RMFSolid : public RMFObject
{
  public:
  int nfaces;
  list<list<RMFFace>::iterator> faces;
};

class RMFEntityDef
{
  public:
  char classname[128];
  int flags;
  int nkeys;
  list<RMFKey> keys;
};

class RMFEntity : public RMFObject
{
  public:
  int nsolids;
  list<list<RMFSolid>::iterator> solids;
  RMFVector location;
  RMFEntityDef def;
};

class RMFGroup : public RMFObject
{
  public:
  int nentities;
  list<list<RMFEntity>::iterator> entities;
  int nsolids;
  list<list<RMFSolid>::iterator> solids;
  int ngroups;
  list<RMFGroup> groups;
};

class RMFVisGroup
{
  public:
  char name[128];
  RMFColor color;
  int visible;
};

class RMFCorner
{
  public:
  RMFVector location;
  int index;
  char name[128];
  int nkeys;
  list<RMFKey> keys;
};

enum
{
  RMFPathType_OneWay = 0,
  RMFPathType_Circular = 1,
  RMFPathType_PingPong = 2
};

class RMFPath
{
  public:
  char name[128];
  char classname[128];
  int type;
  int ncorners;
  list<RMFCorner> corners;
};

class RMFMap : public RMFGroup
{
  public:
  int nvisgroups;
  list<RMFVisGroup> visgroups;

  int nverts;
  list<RMFVector> verts;

  int nfaces;
  list<RMFFace> faces;

  int nsolids;
  list<RMFSolid> solids;

  int nentities;
  list<RMFEntity> entities;

  int npaths;
  list<RMFPath> paths;

  int nguides;
  list<RMFVector> guides;

  list<RMFEntity>::iterator worldspawn;
};

void RMFReadMap(FILE *f, RMFMap *map);

#endif

