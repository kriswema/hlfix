/*
 * The contents of this file are copyright 2003 Jedediah Smith
 * <jedediah@silencegreys.com>
 * http://extension.ws/hlfix/
 *
 * This work is licensed under the Creative Commons "Attribution-Share Alike 3.0 Unported" License.
 * To view a copy of this license, visit http://creativecommons.org/licenses/by-sa/3.0/legalcode
 * or, send a letter to Creative Commons, 171 2nd Street, Suite 300, San Francisco, California, 94105, USA.
*/


#ifndef _INC_GEO
#define _INC_GEO

#include <cstdio>
#include <cmath>
#include <list>
#include <cfloat>
#include <cstring>
#include <string>
#include <cstdarg>

extern int FlagGeoDebug, FlagRMFDebug;
extern double GeoEpsilon;
void GeoDebugPrintf(const char *str, ...);

#define foreach(i,list) for ((i) = (list).begin(); (i) != (list).end(); (i)++)
#define rforeach(i,list) for ((i) = (list).rbegin(); (i) != (list).rend(); (i)++)

using namespace std;

extern int GeoCurEntity, GeoCurBrush;

class GeoException
{
  public:
  char *msg;
  int entity, brush;

  GeoException(char *tomsg, ...)
  {
    va_list args;

    msg = new char[1000];

    va_start(args,tomsg);
    vsprintf(msg,tomsg,args);
    va_end(args);

    entity = GeoCurEntity;
    brush = GeoCurBrush;
  }

  ~GeoException()
  {
    delete msg;
  }
};

class GeoVector;
class GeoEdge;
class GeoFace;
class GeoPlane;

inline bool fequal(double a, double b)
{
  return fabs(a-b) <= GeoEpsilon; // after all is said and done, this seems to work the best
}

class GeoVector
{
  public:
  double x, y, z;

  GeoVector() {}
  GeoVector(const GeoVector &v) : x(v.x), y(v.y), z(v.z) {}
  GeoVector(double tx, double ty, double tz) : x(tx), y(ty), z(tz) {}

  void normalize(void)
  {
    double m;

    m = sqrt(x*x + y*y + z*z);
    x /= m;
    y /= m;
    z /= m;
  }

  GeoVector operator+(const GeoVector &v) const
  {
    return GeoVector(x+v.x,y+v.y,z+v.z);
  }

  GeoVector operator-(const GeoVector &v) const
  {
    return GeoVector(x-v.x,y-v.y,z-v.z);
  }

  GeoVector operator-(void) const
  {
    return GeoVector(-x,-y,-z);
  }

  GeoVector operator*(double t) const
  {
    return GeoVector(x*t,y*t,z*t);
  }

  double operator*(const GeoVector v) const
  {
    return x*v.x + y*v.y + z*v.z;
  }

  GeoVector operator%(const GeoVector &v) const
  {
    return GeoVector(
      y*v.z - z*v.y,
      z*v.x - x*v.z,
      x*v.y - y*v.x);
  }

  GeoVector operator=(const GeoVector &v)
  {
    x = v.x;
    y = v.y;
    z = v.z;

    return *this;
  }

  int operator==(const GeoVector &v) const
  {
    return fequal(x,v.x) && fequal(y,v.y) && fequal(z,v.z);
  }

  int operator!=(const GeoVector &v) const
  {
    return !operator==(v);
  }

  int isParallel(const GeoVector &v) const
  {
    GeoVector v1 = v, v2 = *this;

    v1.normalize();
    v2.normalize();

    return v1 % v2 == GeoVector(0,0,0);
  }

  inline int isIn(const GeoPlane &p) const;
  inline int isIn(const GeoEdge &e) const;

  inline int isIn(const GeoEdge &e1, const GeoEdge &e2) const; // tests inside triangle e1.v1, e1.v2, e2.v2 (on boundary IS NOT inside)

  int isIn(const list<GeoEdge> &edges, const GeoVector &norm) const; // tests inside edge cycle.. boundary is NOT inside
  int isIn(const GeoFace &face) const; // tests inside face (inner and outer cycles accounted for).. boundary IS NOT inside
  int isOn(const list<GeoEdge> &edges) const; // tests on boundary of edge cycle

  inline int isColinear(const GeoEdge &e) const;
  inline int sideOf(const GeoPlane &p) const;
};

/*
NOTE: this is a directed plane where norm points away from the front.
This means that (norm,d) is not the same GeoPlane as (-norm,-d)
*/

class GeoPlane
{
  public:
  GeoVector norm;
  double d;

  GeoPlane() {}

  // init with normal and d
  GeoPlane(GeoVector tnorm, double td) : norm(tnorm), d(td) {}

  // init with normal (unnormalized) and point on plane
  GeoPlane(const GeoVector &tnorm, const GeoVector &p)
  {
    norm = tnorm;
    norm.normalize();
    d = -(norm*p);
  }

  // init with 3 points on plane
  GeoPlane(GeoVector v1, GeoVector v2, GeoVector v3)
  {
    norm = v3-v2 % v1-v2;
    norm.normalize();
    d = -(norm*v2);
  }

  GeoPlane(const GeoPlane &p) : norm(p.norm), d(p.d) {}

  /*
  NOTE: According to this function, a plane is equal to its reverse. While this contradicts the comment above, it suits what is currently
  the only use of this function in DecomposeSolids()
  */

  int operator==(const GeoPlane &plane) const
  {
    return (norm == plane.norm && fequal(d,plane.d)) || (norm == -plane.norm && fequal(d,-plane.d));
  }

  const GeoPlane &operator=(const GeoPlane &p)
  {
    norm = p.norm;
    d = p.d;
    return *this;
  }

  void reverse(void)
  {
    norm = -norm;
    d = -d;
  }
};

inline int GeoVector::isIn(const GeoPlane &p) const
{
  return fequal(*this * p.norm,-p.d);
}

inline int GeoVector::sideOf(const GeoPlane &p) const
{
  double d = - (*this * p.norm);

  if (fequal(d,p.d))
    return 0;
  else if (d < p.d)
    return 1;
  else
    return -1;
}

enum
{
  SIDE_BACK = -1,
  SIDE_IN = 0,
  SIDE_FRONT = 1
};

class GeoEdge
{
  public:
  GeoVector v1, v2;
  int index;

  GeoEdge() : index(0) {}

  GeoEdge(GeoVector &tv1, GeoVector &tv2)
  {
    v1 = tv1;
    v2 = tv2;
  }

  int operator==(const GeoEdge &e) const
  {
    return v1 == e.v1 && v2 == e.v2;
  }

  int isReverse(const GeoEdge &e) const
  {
    return v1 == e.v2 && v2 == e.v1;
  }

  int operator!=(const GeoEdge &e) const
  {
    return !operator==(e);
  }

  int isIn(const GeoPlane &p) const
  {
    return v1.isIn(p) && v2.isIn(p);
  }

  int isColinear(const GeoEdge &e) const
  {
    return v1.isColinear(e) && v2.isColinear(e);
  }

  GeoVector intersect(const GeoPlane &p) const
  {
    GeoVector v;
    double t;


    v = v2-v1;
    t = - (p.norm*v1 + p.d) / (p.norm * v);

    #ifdef CD_DEBUG
    printf("        Intersect edge (%g %g %g) to (%g %g %g) with plane (%g %g %g %g) = %g\n",v1.x,v1.y,v1.z,v2.x,v2.y,v2.z,p.norm.x,p.norm.y,p.norm.z,p.d,t);
    #endif

    return v1+(v*t);
  }

  double intersectCo(const GeoPlane &p) const
  {
    return - (p.norm*v1 + p.d) / (p.norm * (v2-v1));
  }

  const GeoVector vec(void) const
  {
    return v2-v1;
  }

  const GeoVector rvec(void) const
  {
    return v1-v2;
  }
};

inline int GeoVector::isIn(const GeoEdge &e) const
{
  return isColinear(e) &&
    (e.v2.x > e.v1.x ? x >= e.v1.x && x <= e.v2.x : x >= e.v2.x && x <= e.v1.x) &&
    (e.v2.y > e.v1.y ? y >= e.v1.y && y <= e.v2.y : y >= e.v2.y && y <= e.v1.y) &&
    (e.v2.z > e.v1.z ? z >= e.v1.z && z <= e.v2.z : z >= e.v2.z && z <= e.v1.z);
}

inline int GeoVector::isColinear(const GeoEdge &e) const
{
  return *this == e.v1 || e.vec().isParallel(*this - e.v1);
}

inline int GeoVector::isIn(const GeoEdge &e1, const GeoEdge &e2) const
{
  GeoVector n;

  n = e2.vec() % e1.rvec(); // get triangle normal

  // test if we are on the inside of each edge of the triangle
  return
    (e1.vec() % (*this - e1.v1)) * n > 0 &&
    (e2.vec() % (*this - e2.v1)) * n > 0 &&
    ((e1.v1 - e2.v2) % (*this - e2.v2)) * n > 0;
}


class GeoTexture
{
  public:
  char texture[256];
  GeoVector uaxis, vaxis;
  float ushift, vshift, uscale, vscale, rot;

  GeoTexture() {}

  GeoTexture(const GeoTexture &gt)
  {
    *this = gt;
  }

  const GeoTexture &operator=(const GeoTexture &gt)
  {
    strncpy(texture,gt.texture,256);
    uaxis = gt.uaxis;
    vaxis = gt.vaxis;
    ushift = gt.ushift;
    vshift = gt.vshift;
    uscale = gt.uscale;
    vscale = gt.vscale;
    rot = gt.rot;
    return *this;
  }

  int operator==(const GeoTexture &gt)
  {
    return
      strcmp(texture,gt.texture) == 0 &&
      uaxis == gt.uaxis &&
      vaxis == gt.vaxis &&
      ushift == gt.ushift &&
      vshift == gt.vshift &&
      uscale == gt.uscale &&
      vscale == gt.vscale &&
      rot == gt.rot;
  }

  int operator!=(const GeoTexture &gt)
  {
    return !operator==(gt);
  }
};

class GeoFace
{
  public:
  list<GeoEdge> edges; // outer edge cycle
  list<list<GeoEdge> > inedges; // inner edge cycles (first cycle is outer)
  GeoVector n;
  GeoTexture tex;
  int index, flag;

  GeoFace() : n(GeoVector(0,0,0)), index(0) {}

  GeoVector calculateNorm(void) const;

  GeoVector norm(void)
  {
    if (n == GeoVector(0,0,0))
      return n = calculateNorm();
    else
      return n;
  }

  GeoVector norm(void) const
  {
    return calculateNorm();
  }

  GeoPlane plane(void) const
  {
    double d, dmax = -DBL_MAX, dmin = DBL_MAX;
    GeoVector tn;
    list<GeoEdge>::const_iterator ie;

    tn = norm();
    tn.normalize();

    foreach (ie,edges)
    {
      d = -(tn*ie->v1);

      if (d > dmax)
        dmax = d;

      if (d < dmin)
        dmin = d;
    }

    return GeoPlane(tn, (dmin + dmax) / 2);
  }

  /*
  // outer edges only
  int isIn(const GeoPlane &plane) const
  {
    list<GeoEdge>::const_iterator ie, je;

    for (ie = edges.begin(); ie != edges.end(); ie++)
      if (ie->isIn(plane))
        break;

    if (ie == edges.end())
      return 0;

    for (je = ie,je++; je != edges.end(); je++)
    {
      if (je->isIn(plane) && !je->isColinear(*ie))
        return 1;
    }

    return 0;
  }
  */

  int isIn(const GeoPlane &plane) const
  {
    list<GeoEdge>::const_iterator ie;

    foreach (ie, edges)
      if (!ie->v1.isIn(plane))
        return 0;

    return 1;
  }

  // outer edges only
  int isPlanar(void) const
  {
    list<GeoEdge>::const_iterator ie;
    GeoPlane p = plane();

    foreach (ie,edges)
      if (!ie->v1.isIn(p))
        return 0;

    return 1;
  }

  // outer edges only
  int isReverse(const GeoFace &face) const
  {
    list<GeoEdge>::const_iterator ie;
    list<GeoEdge>::const_reverse_iterator rje;

    rje = face.edges.rbegin();

    foreach (ie,edges)
      if (ie->isReverse(*rje))
        break;

    if (ie == edges.end())
      return 0;

    while (rje != face.edges.rend())
    {
      if (!ie->isReverse(*rje))
        return 0;

      if (++ie == edges.end())
        ie = edges.begin();

      ++rje;
    }

    return 1;
  }
};

class GeoColor
{
  public:
  unsigned char r, g, b;
};

class GeoVisible
{
  public:
  int visgroup;
  GeoColor color;
};

class GeoKey
{
  public:
  char name[32];
  char value[100];
};

class GeoEntityDef
{
  public:
  char classname[128];
  int flags;
  list<GeoKey> keys;
};

class GeoSolid : public GeoVisible
{
  public:
  list<GeoFace> faces;
  int index;
};

class GeoEntity : public GeoVisible
{
  public:
  list<GeoSolid> solids;
  GeoVector location;
  GeoEntityDef def;
  int index;
};

class GeoGroup : public GeoVisible
{
  public:
  list<GeoSolid> solids;
  list<GeoGroup> groups;
  list<GeoEntity> entities;
  int index;
};

class GeoVisGroup
{
  public:
  char name[128];
  GeoColor color;
  int index;
  int visible;
};

class GeoCorner
{
  public:
  GeoVector location;
  int index;
  char name[128];
  list<GeoKey> keys;
};

enum
{
  RMFPathType_OneWay = 0,
  RMFPathType_Circular = 1,
  RMFPathType_PingPong = 2
};

class GeoPath
{
  public:
  char name[128];
  char classname[128];
  int type;
  list<GeoCorner> corners;
};


class GeoMap : public GeoGroup
{
  public:

  int RMFPos;
  int RMFPosVector;
  int RMFPosSolid;
  int RMFPosEntity;
  int RMFPosGroup;
  int RMFPosFace;
  int RMFPosKey;
  int RMFPosVisGroup;
  int RMFPosPath;
  int RMFPosCorner;

  int MAPVersion;

  // groups, entities and solids inherited from GeoGroup
  GeoEntityDef wsdef; // worldspawn entity definition
  list<GeoVisGroup> visgroups;
  list<GeoPath> paths;
  list<string> wads;

  void RMFRead(FILE *f);
  void RMFReadNString(FILE *f, char *str, int maxlen);
  void RMFReadString(FILE *f, char *str, int len);
  void RMFReadColor(FILE *f, GeoColor *color);
  void RMFReadVector(FILE *f, GeoVector *vector);
  void RMFReadInt(FILE *f, int *i);
  void RMFReadFloat(FILE *f, float *n);
  void RMFReadByte(FILE *f, unsigned char *b);
  void RMFSkip(FILE *f, int d);
  void RMFReadVisible(FILE *f, GeoVisible *visible);
  void RMFReadFace(FILE *f, GeoFace *face);
  void RMFReadSolid(FILE *f, GeoSolid *solid);
  void RMFReadKey(FILE *f, GeoKey *key);
  void RMFReadEntityDef(FILE *f, GeoEntityDef *def);
  void RMFReadEntity(FILE *f, GeoEntity *entity);
  void RMFReadGroup(FILE *f, GeoGroup *group);
  void RMFReadCorner(FILE *f, GeoCorner *corner);
  void RMFReadPath(FILE *f, GeoPath *path);
  void RMFReadVisGroup(FILE *f, GeoVisGroup *vg);

  void RMFWrite(FILE *f);
  void RMFWriteNString(FILE *f, char *str);
  void RMFWriteString(FILE *f, char *str, int len);
  void RMFWriteColor(FILE *f, GeoColor *color);
  void RMFWriteVector(FILE *f, GeoVector *vector);
  void RMFWriteInt(FILE *f, int *pi);
  void RMFWriteFloat(FILE *f, float *pn);
  void RMFWriteByte(FILE *f, unsigned char *pb);
  void RMFWriteInt(FILE *f, int i);
  void RMFWriteFloat(FILE *f, float n);
  void RMFWriteByte(FILE *f, unsigned char b);
  void RMFFill(FILE *f, int d);
  void RMFWriteVisible(FILE *f, GeoVisible *visible);
  void RMFWriteFace(FILE *f, GeoFace *face);
  void RMFWriteSolid(FILE *f, GeoSolid *solid);
  void RMFWriteKey(FILE *f, GeoKey *key);
  void RMFWriteEntityDef(FILE *f, GeoEntityDef *def);
  void RMFWriteEntity(FILE *f, GeoEntity *entity);
  void RMFWriteGroup(FILE *f, GeoGroup *group);
  void RMFWriteCorner(FILE *f, GeoCorner *corner);
  void RMFWritePath(FILE *f, GeoPath *path);
  void RMFWriteVisGroup(FILE *f, GeoVisGroup *vg);

  void MAPWriteKey(FILE *f, GeoKey *k);
  void MAPWriteFace(FILE *f, GeoFace *fa);
  void MAPWriteSolid(FILE *f, GeoSolid *s);
  void MAPWriteEntityDef(FILE *f, GeoEntityDef *def);
  void MAPWriteEntity(FILE *f, GeoEntity *e);
  void MAPWriteGroup(FILE *f, GeoGroup *group, list<GeoEntity *> *ipentities);
  void MAPWritePath(FILE *f, GeoPath *path);
  void MAPWriteCorner(FILE *f, GeoCorner *corner1, GeoCorner *corner2, int index1, int index2, GeoPath *path);
  void MAPWrite(FILE *f);
};


void GenerateTextureInfo(GeoGroup *newgroup, GeoGroup *oldgroup);
void TesselateNonPlanarFaces(GeoGroup *group, GeoGroup *map);
void UniteCoplanarFaces(GeoGroup *group);
void RemoveCoincidentFaces(GeoGroup *group);
void PruneInvisibleObjects(GeoGroup *group, list<GeoVisGroup> *visgroups);
void SnapVertices(GeoGroup *group);
void GeoPrintMessage(const char *str, ...);
void GeoPrintWarning(const char *str, ...);

/*
int CombineDuplicateVertices(RMFMap *map, float tolerance);
int MatchCoincidentFaces(RMFMap *map);
*/

#endif

