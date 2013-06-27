/*
 * The contents of this file are copyright 2003 Jedediah Smith
 * <jedediah@silencegreys.com>
 * http://extension.ws/hlfix/
 *
 * This work is licensed under the Creative Commons "Attribution-Share Alike 3.0 Unported" License.
 * To view a copy of this license, visit http://creativecommons.org/licenses/by-sa/3.0/legalcode
 * or, send a letter to Creative Commons, 171 2nd Street, Suite 300, San Francisco, California, 94105, USA.
*/

#include <list>
#include <map>
#include <cmath>
#include "geo.h"
#include "cd.h"

int FlagGeoDebug;
double GeoEpsilon;

void GeoDebugPrintf(const char *str,...)
{
  if (!FlagGeoDebug)
    return;

  va_list args;

  va_start(args,str);
  vprintf(str,args);
  va_end(args);
  fflush(stdout);
}

int GeoCurEntity, GeoCurBrush;

void GeoPrintMessage(const char *str, ...)
{
  va_list args;

  printf("  (Entity %i, Brush %i): ", GeoCurEntity, GeoCurBrush);

  va_start(args,str);
  vprintf(str,args);
  va_end(args);

  printf("\n");
  fflush(stdout);
}

void GeoPrintWarning(const char *str, ...)
{
  va_list args;

  printf("  WARNING (Entity %i, Brush %i): ", GeoCurEntity, GeoCurBrush);

  va_start(args,str);
  vprintf(str,args);
  va_end(args);

  printf("\n");
  fflush(stdout);
}

int GeoVector::isIn(const list<GeoEdge> &edges, const GeoVector &norm) const
{
  int wind, side1, side2;
  list<GeoEdge>::const_iterator ie;
  GeoPlane
    plane1(norm % edges.front().vec(),*this),
    plane2(norm % plane1.norm, *this);

  foreach (ie,edges)
  {
    side1 = ie->v1.sideOf(plane1);
    side2 = ie->v2.sideOf(plane1);

    if (side1 != side2 && ie->intersect(plane1).sideOf(plane2) == 1)
      if (side1 > side2)
        wind++;
      else
        wind--;
  }

  return wind != 0;
}

int GeoVector::isIn(const GeoFace &face) const
{
  list<list<GeoEdge> >::const_iterator ile;

  foreach (ile, face.inedges)
    if (isIn(*ile,face.norm()) || isOn(*ile))
      return 0;

  return isIn(face.edges,face.norm());
}

int GeoVector::isOn(const list<GeoEdge> &edges) const
{
  list<GeoEdge>::const_iterator ie;

  foreach (ie, edges)
    if (isIn(*ie))
      return 1;

  return 0;
}

GeoVector GeoFace::calculateNorm(void) const
{
  list<GeoEdge>::const_iterator ie;
  GeoVector v(0,0,0);

  for (ie = edges.begin(); ie != edges.end(); ie++)
  {
    v.x -= (ie->v1.z + ie->v2.z) * (ie->v2.y - ie->v1.y);
    v.y -= (ie->v1.x + ie->v2.x) * (ie->v2.z - ie->v1.z);
    v.z -= (ie->v1.y + ie->v2.y) * (ie->v2.x - ie->v1.x);
  }

  v.normalize();
  return v;
}


void GenerateFaceTextureInfo(GeoFace *face, GeoGroup *oldgroup)
{
  strcpy(face->tex.texture,"AAATRIGGER");

  face->tex.uaxis = face->edges.front().vec();
  face->tex.uaxis.normalize();
  face->tex.vaxis = face->norm() % face->tex.uaxis;
  face->tex.vaxis.normalize();

  face->tex.ushift = 0;
  face->tex.vshift = 0;
  face->tex.uscale = 1;
  face->tex.vscale = 1;
  face->tex.rot = 0;
}

void GenerateTextureInfo(GeoGroup *newgroup, GeoGroup *oldgroup)
{
  list<GeoGroup>::iterator igroup;
  list<GeoEntity>::iterator ientity;
  list<GeoSolid>::iterator isolid;
  list<GeoFace>::iterator iface;

  for (igroup = newgroup->groups.begin(); igroup != newgroup->groups.end(); igroup++)
    GenerateTextureInfo(&*igroup,oldgroup);

  for (ientity = newgroup->entities.begin(); ientity != newgroup->entities.end(); ientity++)
    for (isolid = ientity->solids.begin(); isolid != ientity->solids.end(); isolid++)
      for (iface = isolid->faces.begin(); iface != isolid->faces.end(); iface++)
        GenerateFaceTextureInfo(&*iface,oldgroup);

  for (isolid = newgroup->solids.begin(); isolid != newgroup->solids.end(); isolid++)
    for (iface = isolid->faces.begin(); iface != isolid->faces.end(); iface++)
      GenerateFaceTextureInfo(&*iface,oldgroup);
}

int FindReverseFace(GeoGroup &group, GeoFace &face, list<GeoFace> **rfaces, list<GeoFace>::iterator *irface)
{
  list<GeoFace>::iterator iface;
  list<GeoSolid>::iterator isolid;
  list<GeoEntity>::iterator ientity;
  list<GeoGroup>::iterator igroup;

  foreach (igroup, group.groups)
    if (FindReverseFace(*igroup,face,rfaces,irface))
      return 1;

  foreach (ientity, group.entities)
    foreach (isolid, ientity->solids)
      foreach (iface, isolid->faces)
        if (iface->isReverse(face))
        {
          *rfaces = &isolid->faces;
          *irface = iface;
          return 1;
        }

  foreach (isolid, group.solids)
    foreach (iface, isolid->faces)
      if (iface->isReverse(face))
      {
        *rfaces = &isolid->faces;
        *irface = iface;
        return 1;
      }

  return 0;
}

void TesselateNonPlanarFace(list<GeoFace> *faces, list<GeoFace>::iterator iface, list<GeoFace> *rfaces, list<GeoFace>::iterator irface)
{
  GeoVector n, v;
  GeoPlane plane1, plane2, plane3;
  GeoEdge edge;
  GeoFace face, rface;
  list<GeoEdge>::iterator ie1, ie2, ieFirst, je;
  list<GeoEdge>::reverse_iterator ier1, ier2;
  double t;
  int nverts, s1, s2, validEar, index = 1;

  n = iface->norm();
  nverts = iface->edges.size();
  ieFirst = ie1 = iface->edges.begin();

  if (rfaces != NULL)
    rforeach (ier1, irface->edges)
      if (ier1->isReverse(*ie1))
        break;

  if (FlagGeoDebug)
  {
    plane1 = iface->plane();

    GeoDebugPrintf("Tesselating non-planar face [%s] with %i vertices and plane [%lg %lg %lg %lg]\n",
      iface->tex.texture, nverts,
      plane1.norm.x, plane1.norm.y, plane1.norm.z, plane1.d);
  }

  while (nverts > 3) // while polyline has more than 3 vertices
  {
    ie2 = ie1;

    if (++ie2 == iface->edges.end())
      ie2 = iface->edges.begin();

    if (rfaces != NULL)
    {
      ier2 = ier1;

      if (++ier2 == irface->edges.rend())
        ier2 = irface->edges.rbegin();
    }

    GeoDebugPrintf("  Testing ear at [%lg %lg %lg]\n",ie1->v2.x,ie1->v2.y,ie1->v2.z);

    validEar = 0;

    if ((ie2->vec() % ie1->rvec()) * n > 0) // if ie1 and ie2 form a convex corner
    {
      validEar = 1;
      plane1 = GeoPlane(n % ie1->vec(),ie1->v1);
      plane2 = GeoPlane(n % ie2->vec(),ie2->v1);
      plane3 = GeoPlane(n % (ie1->v1 - ie2->v2),ie2->v2);

      GeoDebugPrintf("    Ear is convex.. planes are [%lg %lg %lg %lg] [%lg %lg %lg %lg] [%lg %lg %lg %lg]\n",
        plane1.norm.x,plane1.norm.y,plane1.norm.z,plane1.d,
        plane2.norm.x,plane2.norm.y,plane1.norm.z,plane2.d,
        plane3.norm.x,plane3.norm.y,plane1.norm.z,plane3.d);

      foreach (je,iface->edges) // for each edge je
      {
        GeoDebugPrintf("      Testing edge [%lg %lg %lg] to [%lg %lg %lg]\n",je->v1.x,je->v1.y,je->v1.z,je->v2.x,je->v2.y,je->v2.z);

        if (je->v1.sideOf(plane1) == 1 && je->v1.sideOf(plane2) == 1 && je->v1.sideOf(plane3) == 1)
        {
          GeoDebugPrintf("        Edge.v1 is in projected triangle\n");

          validEar = 0;
          break;
        }
      }
    }

    if (validEar) // if there are no intersections, this is a valid ear and we clip it
    {
      face = GeoFace();
      face.index = index;
      face.tex = iface->tex;

      edge = *ie1;
      edge.index = 0;
      face.edges.push_back(edge);

      edge = *ie2;
      edge.index = 1;
      face.edges.push_back(edge);

      edge = GeoEdge(ie2->v2,ie1->v1);
      edge.index = 2;
      face.edges.push_back(edge);

      faces->push_back(face);

      ie1->v2 = ie2->v2;
      iface->edges.erase(ie2);
      iface->calculateNorm();
      ieFirst = ie1;


      if (FlagGeoDebug)
      {
        GeoDebugPrintf("    Clipping ear with normal [%lg %lg %lg]\n",face.norm().x,face.norm().y,face.norm().z);

        foreach (ie2, face.edges)
          GeoDebugPrintf("      [%lg %lg %lg] to [%lg %lg %lg]\n", ie2->v1.x, ie2->v1.y, ie2->v1.z, ie2->v2.x, ie2->v2.y, ie2->v2.z);
      }

      if (rfaces != NULL)
      {
        rface = GeoFace();
        rface.index = index;
        rface.tex = irface->tex;

        edge = GeoEdge(ier1->v2,ier2->v1);
        edge.index = 0;
        rface.edges.push_back(edge);

        edge = *ier2;
        edge.index = 1;
        rface.edges.push_back(edge);

        edge = *ier1;
        edge.index = 2;
        rface.edges.push_back(edge);

        rfaces->push_back(rface);

        ier2->v2 = ier1->v2;

        if ((ie2 = ier2.base()) == irface->edges.end())
          ie2 = irface->edges.begin();

        irface->edges.erase(ie2); // NOTE: this actually erases ier1
        irface->calculateNorm();

        if (FlagGeoDebug)
        {
          GeoDebugPrintf("    Clipping reverse ear with normal [%lg %lg %lg]\n",rface.norm().x,rface.norm().y,rface.norm().z);

          foreach (ie2, rface.edges)
            GeoDebugPrintf("      [%lg %lg %lg] to [%lg %lg %lg]\n", ie2->v1.x, ie2->v1.y, ie2->v1.z, ie2->v2.x, ie2->v2.y, ie2->v2.z);
        }
      }

      --nverts;
      ++index;

      GeoDebugPrintf("    Clipping valid ear.. %i vertices remain\n",nverts);
    }
    else
    {
      if (++ie1 == iface->edges.end())
        ie1 = iface->edges.begin();

      if (ie1 == ieFirst)
        throw new GeoException((char *)"Could not tesselate non-planar face");

      if (rfaces != NULL && ++ier1 == irface->edges.rend())
        ier1 = irface->edges.rbegin();
    }
  }

  if (FlagGeoDebug)
  {
    GeoDebugPrintf("  Remaining ear normal [%lg %lg %lg]\n", iface->norm().x, iface->norm().y, iface->norm().z);

    foreach (ie1, iface->edges)
      GeoDebugPrintf("      [%lg %lg %lg] to [%lg %lg %lg]\n", ie1->v1.x, ie1->v1.y, ie1->v1.z, ie1->v2.x, ie1->v2.y, ie1->v2.z);

    if (rfaces != NULL)
    {
      GeoDebugPrintf("  Remaining reverse ear normal [%lg %lg %lg]\n", irface->norm().x, irface->norm().y, irface->norm().z);
      foreach (ie1, irface->edges)
        GeoDebugPrintf("      [%lg %lg %lg] to [%lg %lg %lg]\n", ie1->v1.x, ie1->v1.y, ie1->v1.z, ie1->v2.x, ie1->v2.y, ie1->v2.z);

    }
  }
}

void TesselateNonPlanarFaces(GeoGroup *group, GeoGroup *map)
{
  list<GeoGroup>::iterator igroup;
  list<GeoEntity>::iterator ientity;
  list<GeoSolid>::iterator isolid;
  list<GeoFace>::iterator iface;
  list<GeoFace> *rfaces;
  list<GeoFace>::iterator irface;

  for (igroup = group->groups.begin(); igroup != group->groups.end(); igroup++)
    TesselateNonPlanarFaces(&*igroup,map);

  for (ientity = group->entities.begin(); ientity != group->entities.end(); ientity++)
  {
    for (isolid = ientity->solids.begin(); isolid != ientity->solids.end(); isolid++)
    {
      GeoCurEntity = ientity->index;

      for (iface = isolid->faces.begin(); iface != isolid->faces.end(); iface++)
      {
        GeoCurBrush = isolid->index;

        if (!iface->isPlanar())
        {
          GeoDebugPrintf("Found non-planar face [%s] with normal [%lg %lg %lg]\n",iface->tex.texture,iface->norm().x,iface->norm().y,iface->norm().z);

          if (FindReverseFace(*map,*iface,&rfaces,&irface))
          {
            GeoDebugPrintf("  Found reverse face [%s]\n",irface->tex.texture);

            GeoPrintMessage("Tesselating non-planar face (also tesselating reverse face)");
            TesselateNonPlanarFace(&isolid->faces,iface,rfaces,irface);
          }
          else
          {
            GeoPrintMessage("Tesselating non-planar face");
            TesselateNonPlanarFace(&isolid->faces,iface,NULL,irface);
          }
        }
      }
    }
  }

  GeoCurEntity = 0;

  for (isolid = group->solids.begin(); isolid != group->solids.end(); isolid++)
  {
    GeoCurBrush = isolid->index;

    for (iface = isolid->faces.begin(); iface != isolid->faces.end(); iface++)
    {
      if (!iface->isPlanar())
      {
        if (FindReverseFace(*map,*iface,&rfaces,&irface))
        {
          GeoPrintMessage("Tesselating non-planar face (also tesselating reverse face)");
          TesselateNonPlanarFace(&isolid->faces,iface,rfaces,irface);
        }
        else
        {
          GeoPrintMessage("Tesselating non-planar face");
          TesselateNonPlanarFace(&isolid->faces,iface,NULL,irface);
        }
      }
    }
  }
}


// convex solids only
void UniteCoplanarFaces(GeoSolid *solid)
{
  list<GeoFace>::iterator iface, jface;
  list<GeoEdge>::iterator ie, je;
  list<GeoEdge> edges;
  list<GeoFace> faces;
  int rebuild, texwarning;
  GeoPlane plane;

  GeoDebugPrintf("  Finding coplanar faces in solid %i\n", solid->index);

  for (iface = solid->faces.begin(); iface != solid->faces.end();)
  {
    plane = iface->plane();
    edges.clear();
    rebuild = 0;
    texwarning = 0;

    GeoDebugPrintf("    Testing face %i [%s] with normal [%lg %lg %lg]\n", iface->index, iface->tex.texture, plane.norm.x, plane.norm.y, plane.norm.z);

    for (jface = iface, ++jface; jface != solid->faces.end(); jface++)
    {
      if (jface->isIn(plane) && jface->norm() * plane.norm > 0)
      {
        GeoDebugPrintf("      Found coplanar face %i [%s]\n", jface->index, jface->tex.texture);

        foreach (ie,jface->edges)
          edges.push_back(*ie);

        rebuild = 1;

        if (jface->tex != iface->tex)
          texwarning = 1;
      }
    }

    if (rebuild)
    {
      foreach (ie,iface->edges)
        edges.push_back(*ie);

      GeoDebugPrintf("    Removing coincident and colinear edges out of %i\n",edges.size());

      rebuild = 0;

      for (ie = edges.begin(); ie != edges.end();)
      {
        for (je = ie, ++je; je != edges.end(); je++)
          if (ie->isReverse(*je))
            break;

        if (je != edges.end())
        {
          GeoDebugPrintf("        Removing coincident edge [%lg %lg %lg] to [%lg %lg %lg]\n", ie->v1.x, ie->v1.y, ie->v1.z, ie->v2.x, ie->v2.y, ie->v2.z);

          edges.erase(je);
          edges.erase(ie++);
          rebuild = 1;
        }
        else
          ++ie;
      }
    }

    if (rebuild)
    {
      GeoPrintMessage("Uniting coplanar faces");

      if (texwarning)
        GeoPrintWarning("Uniting faces with different texture info");

      GeoDebugPrintf("    Rebuilding faces in plane\n");

      GenerateFaces(&edges,plane.norm,&faces,iface->tex);
      ++iface;

      GeoDebugPrintf("    Erasing old faces in plane\n");

      for (jface = solid->faces.begin(); jface != solid->faces.end();)
      {
        if (jface->isIn(plane))
        {
          if (iface == jface)
            ++iface;

          solid->faces.erase(jface++);
        }
        else
          ++jface;
      }
    }
    else
      ++iface;
  }

  foreach (iface, faces)
  {
    foreach (ie, iface->edges)
    {
      je = ie;

      if (++je == iface->edges.end())
        je = iface->edges.begin();

      while (ie->isColinear(*je))
      {
        GeoDebugPrintf("        Uniting colinear edges [%lg %lg %lg] to [%lg %lg %lg] and [%lg %lg %lg] to [%lg %lg %lg]\n",
          ie->v1.x, ie->v1.y, ie->v1.z, ie->v2.x, ie->v2.y, ie->v2.z,
          je->v1.x, je->v1.y, je->v1.z, je->v2.x, je->v2.y, je->v2.z);
        ie->v2 = je->v2;
        edges.erase(je++);

        if (je == iface->edges.end())
          je = iface->edges.begin();
      }
    }
  }

  solid->faces.splice(solid->faces.end(),faces);
}

void UniteCoplanarFaces(GeoGroup *group)
{
  list<GeoGroup>::iterator igroup;
  list<GeoEntity>::iterator ientity;
  list<GeoSolid>::iterator isolid;

  foreach (igroup,group->groups)
    UniteCoplanarFaces(&*igroup);

  foreach (ientity,group->entities)
  {
    GeoCurEntity = ientity->index;

    foreach (isolid,ientity->solids)
    {
      GeoCurBrush = isolid->index;
      UniteCoplanarFaces(&*isolid);
    }
  }

  GeoCurEntity = 0;

  foreach (isolid,group->solids)
  {
    GeoCurBrush = isolid->index;
    UniteCoplanarFaces(&*isolid);
  }
}

void PruneInvisibleObjects(GeoGroup *group, map<int,char> &vis)
{
  list<GeoGroup>::iterator igroup;
  list<GeoEntity>::iterator ientity;
  list<GeoSolid>::iterator isolid;

  for (igroup = group->groups.begin(); igroup != group->groups.end();)
  {
    if (vis[igroup->visgroup])
    {
      PruneInvisibleObjects(&*igroup,vis);
      ++igroup;
    }
    else
    {
      GeoDebugPrintf("  Erasing group %i\n",igroup->index);
      group->groups.erase(igroup++);
    }
  }

  for (ientity = group->entities.begin(); ientity != group->entities.end();)
  {
    if (vis[ientity->visgroup])
    {
      for (isolid = ientity->solids.begin(); isolid != ientity->solids.end();)
      {
        if (vis[isolid->visgroup])
          ++isolid;
        else
        {
          GeoDebugPrintf("  Erasing solid %i\n",isolid->index);
          ientity->solids.erase(isolid++);
        }
      }

      ++ientity;
    }
    else
    {
      GeoDebugPrintf("  Erasing entity %i\n",ientity->index);
      group->entities.erase(ientity++);
    }
  }

  for (isolid = group->solids.begin(); isolid != group->solids.end();)
  {
    if (vis[isolid->visgroup])
      ++isolid;
    else
    {
      GeoDebugPrintf("  Erasing solid %i\n",isolid->index);
      group->solids.erase(isolid++);
    }
  }
}

void PruneInvisibleObjects(GeoGroup *group, list<GeoVisGroup> *visgroups)
{
  list<GeoVisGroup>::iterator ivg;
  map<int,char> vis;

  vis[0] = char(1);

  foreach (ivg, *visgroups)
    vis[ivg->index] = char(ivg->visible);

  PruneInvisibleObjects(group,vis);
}

void SnapVertices(GeoSolid *solid)
{
  list<GeoFace>::iterator iface, jface;
  list<GeoEdge>::iterator ie, je;
  GeoVector v;

  foreach (iface, solid->faces)
    foreach (ie, iface->edges)
      for (jface = iface, ++jface; jface != solid->faces.end(); jface++)
        foreach (je, jface->edges)
        {
          v = je->v1 - ie->v1;

          if (fabs(v.x) < 0.1 && fabs(v.y) < 0.1 && fabs(v.z) < 0.1)
            je->v1 = ie->v1;

          v = je->v2 - ie->v1;

          if (fabs(v.x) < 0.1 && fabs(v.y) < 0.1 && fabs(v.z) < 0.1)
            je->v2 = ie->v1;
        }
}

void SnapVertices(GeoGroup *group)
{
  list<GeoGroup>::iterator igroup;
  list<GeoEntity>::iterator ientity;
  list<GeoSolid>::iterator isolid;

  foreach (igroup, group->groups)
    SnapVertices(&*igroup);

  foreach (ientity, group->entities)
    foreach (isolid, ientity->solids)
      SnapVertices(&*isolid);

  foreach (isolid, group->solids)
    SnapVertices(&*isolid);
}
