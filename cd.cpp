/*
 * The contents of this file are copyright 2003 Jedediah Smith
 * <jedediah@silencegreys.com>
 * http://extension.ws/hlfix/
 *
 * This work is licensed under the Creative Commons "Attribution-Share Alike 3.0 Unported" License.
 * To view a copy of this license, visit http://creativecommons.org/licenses/by-sa/3.0/legalcode
 * or, send a letter to Creative Commons, 171 2nd Street, Suite 300, San Francisco, California, 94105, USA.
*/

#include <cmath>
#include <list>
#include <set>
#include <map>
#include "geo.h"
#include "cd.h"

using namespace std;

/*
Returns a number which increases as the internal angle between a and b increases:
if 0   < theta <= 90  then 0 < ret <= 1
if 90  < theta <= 180 then 1 < ret <= 2
if 180 < theta <= 270 then 2 < ret <= 3
if 270 < theta <= 360 then 3 < ret <= 4
The arc of the internal angle is to the left of a when looking from -norm
If a and b are parallel, the internal angle is considered 360 degrees
*/

double InternalAngle(GeoVector a, GeoVector b, GeoVector norm)
{
  a.normalize();
  b.normalize();

  if ((a % b) * norm > 0)
    return 1 - (a * b);
  else
    return 3 + (a * b);
}

class VertexIsLeftOf
{
  public:
  GeoVector n;

  VertexIsLeftOf(GeoVector tn) : n(tn) { }

  bool operator()(const GeoVector &a, const GeoVector &b) const
  {
    return a*n < b*n;
  }
};

void GenerateCutEdges(GeoFace &face, GeoPlane &cutplane, list<GeoEdge> *frontEdges, list<GeoEdge> *backEdges, list<GeoEdge> *frontPlaneEdges, list<GeoEdge> *backPlaneEdges)
{
  list<GeoEdge>::iterator ieFirst, ie, ieBegin, ieEnd;
  list<list<GeoEdge> >::iterator ile;
  VertexIsLeftOf comp(cutplane.norm % face.norm());
  multiset<GeoVector,VertexIsLeftOf> backVerts(comp), frontVerts(comp);
  multiset<GeoVector,VertexIsLeftOf>::iterator vi;
  GeoVector v;
  GeoEdge e;

  GeoDebugPrintf("      Cut plane is (%gx + %gy + %gz + %g) = 0\n",cutplane.norm.x,cutplane.norm.y,cutplane.norm.z,cutplane.d);
  GeoDebugPrintf("      Cut line normal is (%g %g %g)\n",comp.n.x,comp.n.y,comp.n.z);

  ile = face.inedges.begin();

  for (;;)
  {
    if (ile == face.inedges.end())
    {
      ieBegin = ie = face.edges.begin();
      ieEnd = face.edges.end();

      while (ie->isIn(cutplane))
        if (++ie == face.edges.end())
          throw new GeoException((char *)"Attempt to cut outer edge cycle which lies entirely within cutting plane");

      GeoDebugPrintf("      Starting at edge %i of outer cycle\n",ie->index);
    }
    else
    {
      ieBegin = ie = ile->begin();
      ieEnd = ile->end();

      while (ie->isIn(cutplane))
        if (++ie == ile->end())
          throw new GeoException((char *)"Attempt to cut inner edge cycle which lies entirely within cutting plane");

      GeoDebugPrintf("      Starting at edge %i of an inner cycle\n",ie->index);
    }

    ieFirst = ie;

    do
    {
      switch(ie->v1.sideOf(cutplane) * 3 + ie->v2.sideOf(cutplane))
      {
        case SIDE_BACK*3+SIDE_BACK:
          backEdges->push_back(*ie);
        break;

        case SIDE_BACK*3+SIDE_FRONT:
          v = ie->intersect(cutplane);
          backEdges->push_back(GeoEdge(ie->v1,v));
          frontEdges->push_back(GeoEdge(v,ie->v2));
          backVerts.insert(v);
          frontVerts.insert(v);

          GeoDebugPrintf("        Edge %i crossing to FRONT of cut plane at (%g %g %g)\n",ie->index,v.x,v.y,v.z);
        break;

        case SIDE_BACK*3+SIDE_IN:
          backEdges->push_back(*ie);
          backVerts.insert(ie->v2);

          GeoDebugPrintf("        Edge %i encounter BACK of cut plane at (%g %g %g)\n",ie->index,ie->v2.x,ie->v2.y,ie->v2.z);
        break;

        case SIDE_FRONT*3+SIDE_FRONT:
          frontEdges->push_back(*ie);
        break;

        case SIDE_FRONT*3+SIDE_BACK:
          v = ie->intersect(cutplane);
          frontEdges->push_back(GeoEdge(ie->v1,v));
          backEdges->push_back(GeoEdge(v,ie->v2));
          frontVerts.insert(v);
          backVerts.insert(v);

          GeoDebugPrintf("        Edge %i crossing to BACK of cut plane at (%g %g %g)\n",ie->index,v.x,v.y,v.z);
        break;

        case SIDE_FRONT*3+SIDE_IN:
          frontEdges->push_back(*ie);
          frontVerts.insert(ie->v2);

          GeoDebugPrintf("        Edge %i encounter FRONT of cut plane at (%g %g %g)\n",ie->index,ie->v2.x,ie->v2.y,ie->v2.z);
        break;

        case SIDE_IN*3+SIDE_FRONT:
          frontEdges->push_back(*ie);
          frontVerts.insert(ie->v1);

          GeoDebugPrintf("        Edge %i leaving FRONT of cut plane at (%g %g %g)\n",ie->index,ie->v1.x,ie->v1.y,ie->v1.z);
        break;

        case SIDE_IN*3+SIDE_BACK:
          backEdges->push_back(*ie);
          backVerts.insert(ie->v1);

          GeoDebugPrintf("        Edge %i leaving BACK of cut plane at (%g %g %g)\n",ie->index,ie->v1.x,ie->v1.y,ie->v1.z);
        break;
      }

      if (++ie == ieEnd)
        ie = ieBegin;
    }
    while (ie != ieFirst);

    if (ile == face.inedges.end())
      break;

    ++ile;
  }

  GeoDebugPrintf("\n      Generating edges for %i front intersection points\n", frontVerts.size());

  if (frontVerts.size() & 1) throw new GeoException((char *)"Odd number of front intersection points");

  for (vi = frontVerts.begin(); vi != frontVerts.end();)
  {
    e.v1 = *vi;
    vi++;
    e.v2 = *vi;
    vi++;

    if (e.v1 == e.v2)
    {
      GeoDebugPrintf("        Skipping zero-length edge at (%g %g %g)\n",e.v1.x,e.v1.y,e.v1.z);
    }
    else
    {
      GeoDebugPrintf("        Adding front edge from (%g %g %g) to (%g %g %g)\n",e.v1.x,e.v1.y,e.v1.z,e.v2.x,e.v2.y,e.v2.z);

      frontEdges->push_back(e);
      frontPlaneEdges->push_back(GeoEdge(e.v2,e.v1));
    }
  }

  GeoDebugPrintf("\n      Generating edges for %i back intersection points\n", backVerts.size());

  if (backVerts.size() & 1) throw new GeoException((char *)"Odd number of back intersection points");

  for (vi = backVerts.begin(); vi != backVerts.end();)
  {
    e.v2 = *vi;
    vi++;
    e.v1 = *vi;
    vi++;

    if (e.v1 == e.v2)
    {
      GeoDebugPrintf("        Skipping zero-length edge at (%g %g %g)\n",e.v1.x,e.v1.y,e.v1.z);
    }
    else
    {
      GeoDebugPrintf("        Adding back edge from (%g %g %g) to (%g %g %g)\n",e.v1.x,e.v1.y,e.v1.z,e.v2.x,e.v2.y,e.v2.z);

      backEdges->push_back(e);
      backPlaneEdges->push_back(GeoEdge(e.v2,e.v1));
    }
  }
}

void FindAdjacentEdges(list<GeoEdge> *edges, list<GeoEdge>::iterator ieStart, list<GeoEdge> *inedges, GeoVector norm)
{
  list<GeoEdge>::iterator ieAdjacent, ie;
  GeoEdge edge;
  GeoVector v;
  double aAdjacent, a;
  int index;

  edge = *ieStart;
  edges->erase(ieStart);
  edge.index = 0;
  inedges->push_back(edge);

  v = edge.v1;
  index = 1;

  GeoDebugPrintf("        First edge is (%g %g %g) to (%g %g %g)\n", edge.v1.x,edge.v1.y,edge.v1.z,edge.v2.x,edge.v2.y,edge.v2.z);
  fflush(stdout);

  while (edge.v2 != v)
  {
    aAdjacent = 999.0;

    for (ie = edges->begin(); ie != edges->end(); ie++)
    {
      a = InternalAngle(ie->vec(),edge.rvec(),norm);

      GeoDebugPrintf("          Testing edge (%g %g %g) to (%g %g %g) with internal angle %g\n",ie->v1.x,ie->v1.y,ie->v1.z,ie->v2.x,ie->v2.y,ie->v2.z,a);

      if (ie->v1 == edge.v2 && a < aAdjacent)
      {
        GeoDebugPrintf("            Edge is adjacent and has smallest angle so far\n");
        aAdjacent = a;
        ieAdjacent = ie;
      }
    }

    if (aAdjacent == 999.0)
      throw new GeoException((char *)"Attempt to assemble incomplete edge cycle");

    edge = *ieAdjacent;
    edges->erase(ieAdjacent);
    edge.index = index++;
    inedges->push_back(edge);

    GeoDebugPrintf("        Adding edge (%g %g %g) to (%g %g %g)\n", edge.v1.x,edge.v1.y,edge.v1.z,edge.v2.x,edge.v2.y,edge.v2.z);
    fflush(stdout);
  }
}

/*
jface is inside iface if
  (all vertices of jface are on the boundary of iface) or
  (any vertex of jface is
    (inside iface and
    not inside an inner cycle of iface and
    not on the boundary of an inner cycle of iface))
*/

void FindTexture(GeoFace *face, list<GeoFace> *faces)
{
  list<GeoFace>::iterator jface, ifaceIn;
  list<GeoEdge>::iterator ie, je;
  list<GeoEdge>::iterator ile;
  int onBoundary, inFaces;;

  GeoDebugPrintf("      Finding texture for face %i\n", face->index);

  ifaceIn = faces->end();
  inFaces = 0;

  foreach (jface, *faces)
  {
    foreach (je, jface->edges)
      if (!je->v1.isOn(face->edges))
        break;

    if (je == jface->edges.end())
    {
      GeoDebugPrintf("        Found contained face with texture [%s]\n",jface->tex.texture);

      if (inFaces == 0)
      {
        face->tex = jface->tex;
        ++inFaces;
      }
      else if (face->tex != jface->tex)
        ++inFaces;
    }
    else
    {
      foreach (je, jface->edges)
        if (je->v1.isIn(*face))
          break;

      if (je != jface->edges.end() && face->tex != jface->tex)
      {
        GeoDebugPrintf("        Found contained face with texture [%s]\n",jface->tex.texture);

        if (inFaces == 0)
        {
          face->tex = jface->tex;
          ++inFaces;
        }
        else if (face->tex != jface->tex)
          ++inFaces;
      }
    }
  }

  if (inFaces > 1)
    GeoPrintWarning("Found multiple textures for generated face");
}

void FindNestingStructure(list<GeoFace> *faces, const GeoVector &norm)
{
  list<GeoFace>::iterator iface, jface;

  for (iface = faces->begin(); iface != faces->end();)
  {
    if (iface->norm() * norm <= 0) // if this is an inner edge cycle
    {
      GeoDebugPrintf("      Found inner cycle\n");

      foreach (jface,*faces) // search for its containing outer cycle
      {
        if (jface->norm() * norm > 0 && iface->edges.front().v1.isIn(*jface)) // if this is an outer edge cycle and it contains a vertex of the inner cycle
        {
          GeoDebugPrintf("        Found matching containing cycle\n");

          jface->inedges.push_back(iface->edges);
          break;
        }
      }

      if (jface == faces->end())
        throw new GeoException((char *)"Inner edge cycle generated with no containing outer cycle");

      faces->erase(iface++);
    }
    else
      ++iface;
  }
}

void GenerateFaces(list<GeoEdge> *edges, GeoVector norm, list<GeoFace> *faces, const GeoTexture &tex)
{
  GeoFace face;
  list<GeoFace> myfaces;

  face.tex = tex;

  while (!edges->empty())
  {
    GeoDebugPrintf("      Starting new face:\n");

    face.edges.clear();
    FindAdjacentEdges(edges,edges->begin(),&face.edges,norm);

    if (face.edges.size() > 2) // don't add degenerate faces with only 2 edges
      myfaces.push_back(face);
  }

  GeoDebugPrintf("    Finding nesting structure:\n");

  FindNestingStructure(&myfaces,norm);
  faces->splice(faces->end(),myfaces);
}

int FindAdjacentFace(list<GeoFace> *faces, const list<GeoFace>::iterator &iface, const list<GeoEdge>::iterator &ie, list<GeoFace>::iterator *ifaceFound, list<GeoEdge>::iterator *ieFound)
{
  double a, aAdjacent;
  list<GeoFace>::iterator jface, ifAdjacent;
  list<GeoEdge>::iterator je, jeAdjacent;
  list<list<GeoEdge> >::iterator jle;

  aAdjacent = 999;

  foreach (jface, *faces)
  {
    foreach (je, jface->edges)
    {
      if (je->isReverse(*ie))
      {
        a = InternalAngle(iface->norm(),-jface->norm(),je->vec());

        GeoDebugPrintf("          Found adjacent face (%g %g %g) with internal angle %g\n",jface->norm().x,jface->norm().y,jface->norm().z,a);

        if (a < aAdjacent)
        {
          aAdjacent = a;
          ifAdjacent = jface;
          jeAdjacent = je;
        }

        break;
      }
    }

    if (je == jface->edges.end())
    {
      foreach (jle, jface->inedges)
      {
        foreach (je, *jle)
        {
          if (je->isReverse(*ie))
          {
            a = InternalAngle(iface->norm(),-jface->norm(),je->vec());

            GeoDebugPrintf("          Found adjacent face (%g %g %g) with internal angle %g\n",jface->norm().x,jface->norm().y,jface->norm().z,a);

            if (a < aAdjacent)
            {
              aAdjacent = a;
              ifAdjacent = jface;
              jeAdjacent = je;
            }

            break;
          }
        }
      }
    }
  }

  if (aAdjacent == 999)
    return 0;
  else
  {
    if (ieFound != NULL)
      *ieFound = jeAdjacent;

    *ifaceFound = ifAdjacent;
    return 1;
  }
}

void FindAdjacentFaces(list<GeoFace> *faces, list<GeoFace>::iterator iface, int *index)
{
  list<GeoEdge>::iterator ie;
  list<list<GeoEdge> >::iterator ile;
  list<GeoFace>::iterator ifAdjacent;

  iface->index = (*index)++;
  iface->flag = 1;

  GeoDebugPrintf("        Adding face with normal (%g %g %g)\n",iface->norm().x,iface->norm().y,iface->norm().z);

  foreach (ile,iface->inedges)
    foreach (ie,*ile)
      if (FindAdjacentFace(faces,iface,ie,&ifAdjacent,NULL) && !ifAdjacent->flag)
        FindAdjacentFaces(faces,ifAdjacent,index);

   foreach (ie,iface->edges)
      if (FindAdjacentFace(faces,iface,ie,&ifAdjacent,NULL) && !ifAdjacent->flag)
        FindAdjacentFaces(faces,ifAdjacent,index);
}

void GenerateSolids(list<GeoFace> *faces, list<GeoSolid> *solids, GeoColor color, int visgroup, int index)
{
  GeoSolid solid;
  list<GeoFace>::iterator iface, jface;
  list<GeoEdge>::iterator iedge, jedge;
  list<list<GeoEdge> >::iterator iledge;
  int found, nfaces = 0, findex;

  for (iface = faces->begin(); iface != faces->end(); iface++)
  {
    nfaces++;
    iface->flag = 0;
  }

  GeoDebugPrintf("      Generating solids for %i faces\n",nfaces);

  while (nfaces > 0)
  {
    GeoDebugPrintf("      Starting solid\n");

    solid.faces.clear();
    findex = 0;
    FindAdjacentFaces(faces,faces->begin(),&findex);

    for (iface = faces->begin(); iface != faces->end();)
    {
      if (iface->flag)
      {
        solid.faces.push_back(*iface);
        jface = iface++;
        faces->erase(jface);
        nfaces--;
      }
      else
        iface++;
    }

    for (iface = solid.faces.begin(); iface != solid.faces.end(); iface++)
    {
      for (iedge = iface->edges.begin(); iedge != iface->edges.end(); iedge++)
      {
        found = 0;

        for (jface = solid.faces.begin(); found == 0 && jface != solid.faces.end(); jface++)
        {
          for (jedge = jface->edges.begin(); found == 0 && jedge != jface->edges.end(); jedge++)
            if (iedge->isReverse(*jedge))
              found = 1;

          for (iledge = jface->inedges.begin(); found == 0 && iledge != jface->inedges.end(); iledge++)
            for (jedge = iledge->begin(); found == 0 && jedge != iledge->end(); jedge++)
              if (iedge->isReverse(*jedge))
                found = 1;
        }

        if (!found)
          throw new GeoException((char *)"Orphaned face %i [%s] with normal (%g %g %g)",iface->index,iface->tex.texture,iface->norm().x,iface->norm().y,iface->norm().z);
      }
    }

    solid.color = color;
    solid.visgroup = visgroup;
    solid.index = index;
    solids->push_back(solid);
  }
}

void CutSolid(GeoSolid &solid, GeoPlane cutplane, list<GeoSolid> *cutsolids)
{
  list<GeoEdge> edgesFaceFront, edgesFaceBack, edgesCutFront, edgesCutBack;
  list<GeoFace> facesFront, facesBack, facesCutFront, facesCutBack, facesOldCutFront, facesOldCutBack;
  list<GeoFace>::iterator iface;
  GeoTexture tex;

  for (iface = solid.faces.begin(); iface != solid.faces.end(); iface++)
  {
    if (iface->isIn(cutplane))
    {
      if (iface->norm() * cutplane.norm > 0)
        facesOldCutBack.push_back(*iface);
      else
        facesOldCutFront.push_back(*iface);
    }
    else
    {
      GeoDebugPrintf("\n    Cutting face %i [%s] [%lg %lg %lg]\n", iface->index,iface->tex.texture,iface->norm().x,iface->norm().y,iface->norm().z);

      edgesFaceFront.clear();
      edgesFaceBack.clear();
      GenerateCutEdges(*iface,cutplane,&edgesFaceFront,&edgesFaceBack,&edgesCutFront,&edgesCutBack);

      GeoDebugPrintf("\n      Generating faces for front edges\n");

      GenerateFaces(&edgesFaceFront,iface->norm(),&facesFront,iface->tex);

      GeoDebugPrintf("\n      Generating faces for back edges\n");

      GenerateFaces(&edgesFaceBack,iface->norm(),&facesBack,iface->tex);
    }
  }

  strcpy(tex.texture,"NULL");
  tex.ushift = tex.vshift = tex.rot = 0;
  tex.uscale = tex.vscale = 1;
  tex.uaxis = edgesCutFront.begin()->vec() % cutplane.norm;
  tex.vaxis = tex.uaxis % cutplane.norm;
  tex.uaxis.normalize();
  tex.vaxis.normalize();

  GeoDebugPrintf("\n    Generating front cut plane faces\n");

  GenerateFaces(&edgesCutFront,-cutplane.norm,&facesCutFront,tex);

  GeoDebugPrintf("\n    Finding textures for front cut plane faces\n");

  foreach (iface, facesCutFront)
    FindTexture(&*iface,&facesOldCutFront);

  facesFront.splice(facesFront.end(),facesCutFront);

  GeoDebugPrintf("\n    Generating back cut plane faces\n");

  GenerateFaces(&edgesCutBack,cutplane.norm,&facesCutBack,tex);

  GeoDebugPrintf("\n    Finding textures for back cut plane faces\n");

  foreach (iface, facesCutBack)
    FindTexture(&*iface,&facesOldCutBack);

  facesBack.splice(facesBack.end(),facesCutBack);

  GeoDebugPrintf("\n    Generating front solids\n");

  GenerateSolids(&facesFront,cutsolids,solid.color,solid.visgroup,solid.index);

  GeoDebugPrintf("\n    Generating back solids\n");

  GenerateSolids(&facesBack,cutsolids,solid.color,solid.visgroup,solid.index);
}

// return number of edges in iface that are reflex
int ReflexEdges(GeoSolid &solid, list<GeoFace>::iterator iface)
{
  list<GeoFace>::iterator jface;
  list<GeoEdge>::iterator ie, je;
  list<list<GeoEdge> >::iterator ile;
  GeoVector ni, nj;
  int n = 0;

  ni = iface->norm();

  GeoDebugPrintf("    Face normal: (%lg %lg %lg)\n",ni.x, ni.y, ni.z);

  foreach (ie, iface->edges)
  {
    if (!FindAdjacentFace(&solid.faces, iface, ie, &jface, &je))
      throw new GeoException((char *)"Incomplete solid");

    nj = jface->norm();

    GeoDebugPrintf("      Found adjacent face %i (%lg %lg %lg)",jface->index,nj.x,nj.y,nj.z);

    if ((ni % nj) * ie->vec() < 0)
    {
      GeoDebugPrintf(" reflex edge found\n");

      n++;
    }
    else
      GeoDebugPrintf("\n");
  }

  foreach (ile, iface->inedges)
  {
    foreach (ie, *ile)
    {
      if (!FindAdjacentFace(&solid.faces, iface, ie, &jface, &je))
        throw new GeoException((char *)"Incomplete solid");

      nj = jface->norm();

      GeoDebugPrintf("      Found adjacent face %i (%lg %lg %lg)",jface->index,nj.x,nj.y,nj.z);

      if ((ni % nj) * ie->vec() < 0)
      {
        GeoDebugPrintf(" reflex edge found\n");

        n++;
      }
      else
        GeoDebugPrintf("\n");
    }
  }

  return n;
}

int PlaneIsLessThan(const GeoPlane &planea, const GeoPlane &planeb)
{
  GeoPlane pa = planea, pb = planeb;

  if (pa.norm.x + pa.norm.y + pa.norm.z < 0)
    pa.reverse();

  if (pb.norm.x + pb.norm.y + pb.norm.z < 0)
    pb.reverse();

  if (!fequal(pa.norm.x,pb.norm.x))
    return pa.norm.x < pb.norm.x;
  else if (!fequal(pa.norm.y,pb.norm.y))
    return pa.norm.y < pb.norm.y;
  else if (!fequal(pa.norm.z,pb.norm.z))
    return pa.norm.z < pb.norm.z;
  else if (!fequal(pa.d,pb.d)) // necessary because pa.d and pb.d may be fequal even if pa.d<pb.d is true
    return pa.d < pb.d;
  else
    return 0;
}

void DecomposeSolids(list<GeoSolid> *solids)
{
  list<GeoSolid>::iterator isolid;
  list<GeoFace>::iterator iface, ifaceCut;
  map<GeoPlane,int,typeof(PlaneIsLessThan)*> reflexEdges(PlaneIsLessThan);
  GeoPlane plane;
  int r, rmax, nsolids;

  nsolids = solids->size();

  for (isolid = solids->begin(); isolid != solids->end();)
  {
    GeoDebugPrintf("Decomposing Solid:\n  Finding reflex edges:\n");

    GeoCurBrush = isolid->index;
    rmax = 0;
    reflexEdges.clear();

    for (iface = isolid->faces.begin(); iface != isolid->faces.end(); iface++)
    {
      GeoDebugPrintf("\n    Testing reflex edges for face %i [%s]\n",iface->index, iface->tex.texture);

      plane = iface->plane();

      r = ReflexEdges(*isolid,iface);

      if (reflexEdges.find(plane) == reflexEdges.end())
        reflexEdges[plane] = r;
      else
        r = (reflexEdges[plane] += r);

      GeoDebugPrintf("    Plane [%lg %lg %lg %lg] now has %i reflex edges\n",plane.norm.x,plane.norm.y,plane.norm.z,plane.d,r);

      if (r > rmax)
      {
        rmax = r;
        ifaceCut = iface;
      }
    }

    if (rmax != 0)
    {
      GeoDebugPrintf("\n  Cutting along face %i [%s] with %i reflex edges:\n",ifaceCut->index,ifaceCut->tex.texture,rmax);

      if (nsolids > 0)
        GeoPrintMessage("Decomposing non-convex solid");

      CutSolid(*isolid,ifaceCut->plane(),solids);
      solids->erase(isolid++);
    }
    else
      ++isolid;

    --nsolids;
  }
}

void DecomposeGroup(GeoGroup *group)
{
  list<GeoEntity>::iterator ientity;
  list<GeoGroup>::iterator igroup;

  GeoCurEntity = 0;
  DecomposeSolids(&group->solids);

  foreach (ientity, group->entities)
  {
    GeoCurEntity = ientity->index;
    DecomposeSolids(&ientity->solids);
  }

  foreach (igroup, group->groups)
    DecomposeGroup(&*igroup);
}
