/*
 * The contents of this file are copyright 2003 Jedediah Smith
 * <jedediah@silencegreys.com>
 * http://extension.ws/hlfix/
 *
 * This work is licensed under the Creative Commons "Attribution-Share Alike 3.0 Unported" License.
 * To view a copy of this license, visit http://creativecommons.org/licenses/by-sa/3.0/legalcode
 * or, send a letter to Creative Commons, 171 2nd Street, Suite 300, San Francisco, California, 94105, USA.
*/

#include "geo.h"

using namespace std;

void GeoMap::MAPWriteKey(FILE *f, GeoKey *k)
{
  fprintf(f,"\"%s\" \"%s\"\n",k->name,k->value);
}

void GeoMap::MAPWriteFace(FILE *f, GeoFace *face)
{
  list<GeoEdge>::reverse_iterator ire;
  int i;
  float uscale, vscale;

  for (i = 0, ire = face->edges.rbegin(); i < 3; i++, ire++)
    fprintf(f,"( %lg %lg %lg ) ",ire->v1.x,ire->v1.y,ire->v1.z);

  if (MAPVersion == 220)
  {
    fprintf(f,"%s [ %g %g %g %g ] [ %g %g %g %g ] %g %g %g\n",
      face->tex.texture,
      face->tex.uaxis.x,
      face->tex.uaxis.y,
      face->tex.uaxis.z,
      face->tex.ushift,
      face->tex.vaxis.x,
      face->tex.vaxis.y,
      face->tex.vaxis.z,
      face->tex.vshift,
      face->tex.rot,
      face->tex.uscale,
      face->tex.vscale);
  }
  else
  {
    i = 0;
    uscale = face->tex.uscale;
    vscale = face->tex.vscale;

    if (!fequal((face->tex.vaxis % face->tex.uaxis) * face->norm(),1)) throw new GeoException((char *)"texture coordinates not supported with this MAP version");
    if (int(face->tex.ushift) % 16 != 0 || int(face->tex.vshift) % 16 != 0) throw new GeoException((char *)"texture coordinates not supported with this MAP version");

    if (uscale < 0)
    {
      i |= 1;
      uscale = -uscale;
    }

    if (vscale < 0)
    {
      i |= 2;
      vscale = -vscale;
    }

    if (i == 3) i = 7;

    fprintf(f,"%s %i %i %i %g %g\n",
      face->tex.texture,
      int(face->tex.ushift),
      int(face->tex.vshift),
      i, uscale, vscale);
  }
}

void GeoMap::MAPWriteSolid(FILE *f, GeoSolid *solid)
{
  list<GeoFace>::iterator iface;

  GeoCurBrush = solid->index;

  fprintf(f,"{\n");

  foreach (iface, solid->faces)
    MAPWriteFace(f,&*iface);

  fprintf(f,"}\n");
}

void GeoMap::MAPWriteEntityDef(FILE *f, GeoEntityDef *def)
{
  list<GeoKey>::iterator ikey;

  fprintf(f,"\"classname\" \"%s\"\n",def->classname);

  if (def->flags != 0)
    fprintf(f,"\"spawnflags\" \"%i\"\n",def->flags);

  foreach (ikey, def->keys)
    MAPWriteKey(f,&*ikey);
}

void GeoMap::MAPWriteEntity(FILE *f, GeoEntity *entity)
{
  list<GeoSolid>::iterator isolid;

  GeoCurEntity = entity->index;

  fprintf(f,"{\n");

  MAPWriteEntityDef(f,&entity->def);

  if (entity->solids.size() == 0)
    fprintf(f,"\"origin\" \"%lg %lg %lg\"\n",entity->location.x,entity->location.y,entity->location.z);
  else
    foreach (isolid, entity->solids)
      MAPWriteSolid(f,&*isolid);

  fprintf(f,"}\n");
}

void GeoMap::MAPWriteGroup(FILE *f, GeoGroup *group, list<GeoEntity *> *pentities)
{
  list<GeoGroup>::iterator igroup;
  list<GeoEntity>::iterator ientity;
  list<GeoSolid>::iterator isolid;

  foreach (igroup, group->groups)
    MAPWriteGroup(f,&*igroup,pentities);

  foreach (ientity, group->entities)
    pentities->push_back(&*ientity);

  foreach (isolid, group->solids)
    MAPWriteSolid(f,&*isolid);
}

void GeoMap::MAPWriteCorner(FILE *f, GeoCorner *corner1, GeoCorner *corner2, int index1, int index2, GeoPath *path)
{
  list<GeoKey>::iterator ikey;

  fprintf(f,"{\n");
  fprintf(f,"\"classname\" \"%s\"\n",path->classname);

  if (corner1->name[0] == '\0' && index1 != -1)
  {
    GeoDebugPrintf("  Generating corner %s%02i",corner1->name[0] == '\0' ? path->name : corner1->name, index1);
    fprintf(f,"\"targetname\" \"%s%02i\"\n",corner1->name[0] == '\0' ? path->name : corner1->name, index1);
  }
  else
  {
    GeoDebugPrintf("  Generating corner %s",corner1->name[0] == '\0' ? path->name : corner1->name);
    fprintf(f,"\"targetname\" \"%s\"\n",corner1->name[0] == '\0' ? path->name : corner1->name);
  }

  if (corner2 != NULL)
  {
    if (corner2->name[0] == '\0' && index2 != -1)
    {
      GeoDebugPrintf("->%s%02i\n",corner2->name[0] == '\0' ? path->name : corner2->name, index2);
      fprintf(f,"\"target\" \"%s%02i\"\n",corner2->name[0] == '\0' ? path->name : corner2->name, index2);
    }
    else
    {
      GeoDebugPrintf("->%s\n",corner2->name[0] == '\0' ? path->name : corner2->name);
      fprintf(f,"\"target\" \"%s\"\n",corner2->name[0] == '\0' ? path->name : corner2->name);
    }
  }
  else
    GeoDebugPrintf("\n");

  fprintf(f,"\"origin\" \"%lg %lg %lg\"\n",corner1->location.x, corner1->location.y, corner1->location.z);

  // Removed until VHE gets fixed
  /*
  foreach (ikey, corner1->keys)
    MAPWriteKey(f,&*ikey);
  */

  fprintf(f,"}\n");
}

void GeoMap::MAPWritePath(FILE *f, GeoPath *path)
{
  list<GeoCorner>::iterator icorner1, icorner2;
  int maxindex, size, i;
  int *indexes;

  if ((size = path->corners.size()) == 0)
    return;

  if (path->type == RMFPathType_PingPong)
    indexes = new int[size*2-1];
  else
    indexes = new int[size];

  maxindex = i = 0;

  foreach (icorner1, path->corners)
  {
    indexes[i++] = icorner1->index;
    if (icorner1->index > maxindex) maxindex = icorner1->index;
  }

  indexes[0] = -1;

  if (path->type == RMFPathType_PingPong)
  {
    while (i < size*2-1)
      indexes[i++] = ++maxindex;

    indexes[size*2-2] = -1;
  }

  icorner1 = path->corners.begin();
  icorner2 = icorner1;
  ++icorner2;
  i = 0;

  while (icorner2 != path->corners.end())
  {
    MAPWriteCorner(f,&*icorner1,&*icorner2,indexes[i],indexes[i+1],path);

    ++i;
    ++icorner1;
    ++icorner2;
  }

  if (path->type == RMFPathType_OneWay)
    MAPWriteCorner(f,&*icorner1,NULL,indexes[i],0,path);
  else if (path->type == RMFPathType_Circular || size == 1)
    MAPWriteCorner(f,&*icorner1,&path->corners.front(),indexes[i],-1,path);
  else if (path->type == RMFPathType_PingPong && size > 1)
  {
    icorner2 = icorner1;
    --icorner2;

    for (;;)
    {
      MAPWriteCorner(f,&*icorner1,&*icorner2,indexes[i],indexes[i+1],path);

      if (icorner2 == path->corners.begin())
        break;

      ++i;
      --icorner1;
      --icorner2;
    }
  }

  delete indexes;
}

void GeoMap::MAPWrite(FILE *f)
{
  list<string>::iterator istr;
  list<GeoEntity *> pentities;
  list<GeoEntity *>::iterator ipentity;
  list<GeoPath>::iterator ipath;
  int i;

  fprintf(f,"{\n\"mapversion\" \"220\"\n");

   if (!wads.empty())
   {
    fprintf(f,"\"wad\" \"");

      i = 0;

    foreach (istr, wads)
    {
      if (i == 0)
      {
        fprintf(f,"%s",istr->c_str());
        i = 1;
      }
      else
        fprintf(f,";%s",istr->c_str());
    }

    fprintf(f,"\"\n");
  }

  MAPWriteEntityDef(f,&wsdef);
  MAPWriteGroup(f,this,&pentities);
  fprintf(f,"}\n");

  foreach (ipentity, pentities)
    MAPWriteEntity(f,*ipentity);

  foreach (ipath, paths)
    MAPWritePath(f,&*ipath);
}

