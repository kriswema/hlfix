/*
 * The contents of this file are copyright 2003 Jedediah Smith
 * <jedediah@silencegreys.com>
 * http://extension.ws/hlfix/
 *
 * This work is licensed under the Creative Commons "Attribution-Share Alike 3.0 Unported" License.
 * To view a copy of this license, visit http://creativecommons.org/licenses/by-sa/3.0/legalcode
 * or, send a letter to Creative Commons, 171 2nd Street, Suite 300, San Francisco, California, 94105, USA.
*/

#include <cstring>
#include <cstdio>
#include "geo.h"

using namespace std;

int FlagRMFDebug;

void RMFDebugPrintf(const char *str,...)
{
	if (!FlagRMFDebug)
		return;

	va_list args;
		
	va_start(args,str);
	vprintf(str,args);
	va_end(args);
	fflush(stdout);
}

void GeoMap::RMFReadNString(FILE *f, char *str, int maxlen)
{
	unsigned char len;
	
	if (fread(&len,1,1,f) != 1) throw new GeoException("Premature EOF reading NString length");
	if (len > maxlen) throw new GeoException("NString too long");
	if (len == 0) throw new GeoException("Zero length NString");
	if (fread(str,len,1,f) != 1) throw new GeoException("Premature EOF reading NString");
	if (str[len-1] != '\0') throw new GeoException("Unterminated NString");
	RMFPos += len+1;

	//RMFDebugPrintf("NString: %s\n",str);
}

void GeoMap::RMFReadString(FILE *f, char *str, int len)
{
	if (fread(str,len,1,f) != 1) throw new GeoException("Preature EOF reading String");	
	RMFPos += len;

	//RMFDebugPrintf("String: %s\n",str);
}

void GeoMap::RMFReadColor(FILE *f, GeoColor *color)
{
	if (fread(color,3,1,f) != 1) throw new GeoException("Premature EOF reading Color");
	RMFPos += 3;

	//RMFDebugPrintf("Color: %i %i %i\n",(int)color->r,(int)color->g,(int)color->b);
}

void GeoMap::RMFReadVector(FILE *f, GeoVector *vector)
{
	float x, y, z;

	if (fread(&x,4,1,f) != 1) throw new GeoException("Premature EOF reading Vector");
	RMFPos += 4;
	if (fread(&y,4,1,f) != 1) throw new GeoException("Premature EOF reading Vector");
	RMFPos += 4;
	if	(fread(&z,4,1,f) != 1) throw new GeoException("Premature EOF reading Vector");
	RMFPos += 4;
	
	vector->x = x;
	vector->y = y;
	vector->z = z;

	//RMFDebugPrintf("Vector: (%g %g %g)\n",vector->x,vector->y,vector->z);
}

void GeoMap::RMFReadInt(FILE *f, int *i)
{
	if (fread(i,4,1,f) != 1) throw new GeoException("Premature EOF reading int");
	RMFPos += 4;

	//RMFDebugPrintf("Int: %i\n",*i);
}

void GeoMap::RMFReadFloat(FILE *f, float *n)
{
	if (fread(n,4,1,f) != 1) throw new GeoException("Premature EOF reading float");
	RMFPos += 4;

	//RMFDebugPrintf("Float: %g\n",*n);
}

void GeoMap::RMFReadByte(FILE *f, unsigned char *b)
{
	if (fread(b,1,1,f) != 1) throw new GeoException("Premature EOF reading byte");
	RMFPos++;

	//RMFDebugPrintf("Byte: %i\n",*b);
}

void GeoMap::RMFSkip(FILE *f, int d)
{
	if (fseek(f,d,SEEK_CUR) != 0) throw new GeoException("Premature EOF during seek");
	RMFPos += d;

	//RMFDebugPrintf("Skip: %i\n",d);
}

void GeoMap::RMFReadVisible(FILE *f, GeoVisible *visible)
{
	RMFReadInt(f,&visible->visgroup);
	RMFReadColor(f,&visible->color);

	RMFDebugPrintf("Color: %i %i %i\nVisGroup: %i\n",int(visible->color.r),int(visible->color.g),int(visible->color.b),visible->visgroup);
}

void GeoMap::RMFReadFace(FILE *f, GeoFace *face)
{
	int i, nverts;
	GeoVector v, v0;
	GeoEdge e;
	
	RMFReadString(f,face->tex.texture,256);
	RMFSkip(f,4);
	RMFReadVector(f,&face->tex.uaxis);
	RMFReadFloat(f,&face->tex.ushift);
	RMFReadVector(f,&face->tex.vaxis);
	RMFReadFloat(f,&face->tex.vshift);
	RMFReadFloat(f,&face->tex.rot);
	RMFReadFloat(f,&face->tex.uscale);
	RMFReadFloat(f,&face->tex.vscale);
	RMFSkip(f,16);
	RMFReadInt(f,&nverts);
	
	face->edges.clear();

	for (i = 0; i < nverts; i++)
	{
		RMFPosVector = i;

		RMFReadVector(f,&v);
		
		if (i == 0)
			v0 = v;
		else
		{
			e.v1 = v;
			e.index = nverts-i;

			RMFDebugPrintf("Edge: (%g %g %g) to (%g %g %g)\n",e.v1.x,e.v1.y,e.v1.z,e.v2.x,e.v2.y,e.v2.z);
			
			face->edges.push_front(e);
		}
		
		e.v2 = v;
	}
	
	e.v1 = v0;
	e.index = 0;

	RMFDebugPrintf("Edge: (%g %g %g) to (%g %g %g)\n",e.v1.x,e.v1.y,e.v1.z,e.v2.x,e.v2.y,e.v2.z);
			
	face->edges.push_front(e);

	RMFSkip(f,36);
	
	RMFPosVector = -1;
}

void GeoMap::RMFReadSolid(FILE *f, GeoSolid *solid)
{
	int i, nfaces;
	GeoFace fa;
	
	RMFReadVisible(f,solid);
	RMFSkip(f,4);
	RMFReadInt(f,&nfaces);
	solid->faces.clear();

	for (i = 0; i < nfaces; i++)
	{
		RMFPosFace = i;

		RMFDebugPrintf("Face %i:\n",i);
		RMFReadFace(f,&fa);
		fa.index = i;
		solid->faces.push_back(fa);
	}
	
	RMFPosFace = -1;
}

void GeoMap::RMFReadKey(FILE *f, GeoKey *key)
{
	RMFReadNString(f,key->name,32);
	RMFReadNString(f,key->value,100);

	RMFDebugPrintf("Key: \"%s\" = \"%s\"\n",key->name,key->value);
}

void GeoMap::RMFReadEntityDef(FILE *f, GeoEntityDef *def)
{
	int i, nkeys;
	GeoKey k;

	RMFReadNString(f,def->classname,128);
	RMFSkip(f,4);
	RMFReadInt(f,&def->flags);
	RMFReadInt(f,&nkeys);

	RMFDebugPrintf("Classname: %s\nFlags: %i\n",def->classname,def->flags);

	def->keys.clear();

	for (i = 0; i < nkeys; i++)
	{
		RMFPosKey = i;

		RMFReadKey(f,&k);
		def->keys.push_back(k);
	}
	
	RMFPosKey = -1;

	RMFDebugPrintf("");
}

void GeoMap::RMFReadEntity(FILE *f, GeoEntity *entity)
{
	int i, nsolids, tmp;
	GeoSolid s;
	char buf[50];
	
	RMFReadVisible(f,entity);
	RMFReadInt(f,&nsolids);
	entity->solids.clear();

	tmp = RMFPosSolid;

	for (i = 0 ; i < nsolids; i++)
	{
		RMFPosSolid = i;

		RMFReadNString(f,buf,50);
		if (strcmp(buf,"CMapSolid") != 0) throw new GeoException("Expected CMapSolid");

		RMFDebugPrintf("Solid %i:\n",i);

		RMFReadSolid(f,&s);
		s.index = i;
		entity->solids.push_back(s);
	}
	
	RMFPosSolid = tmp;
	
	RMFReadEntityDef(f,&entity->def);
	RMFSkip(f,14);
	RMFReadVector(f,&entity->location);
	RMFSkip(f,4);

	RMFDebugPrintf("Location: %lg %lg %lg\n",entity->location.x, entity->location.y, entity->location.z);
}

void GeoMap::RMFReadGroup(FILE *f, GeoGroup *group)
{
	char buf[50];
	int i, nobjs,tmp;
	GeoSolid s;
	GeoEntity e;
	GeoGroup g;
		
	group->entities.clear();
	group->solids.clear();
	group->groups.clear();
	
	RMFReadVisible(f,group);
	RMFReadInt(f,&nobjs);
	
	for (i = 0; i < nobjs; i++)
	{
		RMFReadNString(f,buf,50);
		
		if (strcmp(buf,"CMapSolid") == 0)
		{
			tmp = RMFPosEntity;
			RMFPosEntity = 0;

			RMFDebugPrintf("Solid %i:\n", RMFPosSolid);

			RMFReadSolid(f,&s);
			s.index = RMFPosSolid++;
			group->solids.push_back(s);

			RMFPosEntity = tmp;
		}
		else if (strcmp(buf,"CMapEntity") == 0)
		{
			RMFDebugPrintf("Entity %i:\n",RMFPosEntity);
			RMFReadEntity(f,&e);
			e.index = RMFPosEntity++;
			group->entities.push_back(e);
		}
		else if (strcmp(buf,"CMapGroup") == 0)
		{
			++RMFPosGroup;
			
			RMFDebugPrintf("Group %i:\n", RMFPosGroup);
			RMFReadGroup(f,&g);
			g.index = RMFPosGroup;
			group->groups.push_back(g);
		}
		else
			throw new GeoException("Invalid world object encountered (expected CMapSolid, CMapEntity or CMapGroup");
	}
}

void GeoMap::RMFReadCorner(FILE *f, GeoCorner *corner)
{
	int i, nkeys;
	GeoKey k;

	RMFReadVector(f,&corner->location);
	RMFReadInt(f,&corner->index);
	RMFReadString(f,corner->name,128);
	RMFReadInt(f,&nkeys);
	
	corner->keys.clear();

	for (i = 0; i < nkeys; i++)
	{
		RMFPosKey = i;

		RMFReadKey(f,&k);
		corner->keys.push_back(k);
	}
	
	RMFPosKey = -1;

	RMFDebugPrintf("");
}

void GeoMap::RMFReadPath(FILE *f, GeoPath *path)
{
	int i, ncorners;
	GeoCorner c;
	
	RMFReadString(f,path->name,128);
	RMFReadString(f,path->classname,128);
	RMFReadInt(f,&path->type);
	RMFReadInt(f,&ncorners);
	
	path->corners.clear();

	for (i = 0; i < ncorners; i++)
	{
		RMFPosCorner = i;

		RMFReadCorner(f,&c);
		path->corners.push_back(c);
	}
	
	RMFPosCorner = -1;

	RMFDebugPrintf("");
}

void GeoMap::RMFReadVisGroup(FILE *f, GeoVisGroup *vg)
{
	unsigned char b;

	RMFReadString(f,vg->name,128);
	RMFReadColor(f,&vg->color);
	RMFSkip(f,1);
	RMFReadInt(f,&vg->index);
	RMFReadByte(f,&b);
	vg->visible = b;
	RMFSkip(f,3);
}

void GeoMap::RMFRead(FILE *f)
{
	char buf[50];
	int i, nvisgroups, npaths;
	GeoVisGroup vg;
	GeoPath p;

	RMFPos = 0;
	RMFPosVector = -1;
	RMFPosSolid = 0;
	RMFPosFace = -1;
	RMFPosEntity = 1;
	RMFPosKey = -1;
	RMFPosVisGroup = -1;
	RMFPosPath = -1;
	RMFPosCorner = -1;
	RMFPosGroup = -1;

	RMFSkip(f,4);
	RMFReadString(f,buf,3);
	if (strncmp(buf,"RMF",3) != 0) throw new GeoException("Invalid header");
	RMFReadInt(f,&nvisgroups);
	
	visgroups.clear();

	for (i = 0; i < nvisgroups; i++)
	{
		RMFPosVisGroup = i;
		RMFReadVisGroup(f,&vg);
		visgroups.push_back(vg);
	}
	
	RMFPosVisGroup = -1;

	RMFReadNString(f,buf,50);
	if (strcmp(buf,"CMapWorld") != 0) throw new GeoException("Expected CMapWorld");

	RMFPosGroup = 0;
	RMFReadGroup(f,this);

	RMFReadEntityDef(f,&wsdef);
	if (strcmp(wsdef.classname,"worldspawn") != 0) throw new GeoException("Root entity is not called \"worldspawn\"");

	RMFSkip(f,12);
	RMFReadInt(f,&npaths);
	
	RMFPosEntity = -1;

	paths.clear();

	for (i = 0; i < npaths; i++)
	{
		RMFPosPath = i;

		RMFReadPath(f,&p);
		paths.push_back(p);
	}
	
	RMFPosPath = -1;
}

void GeoMap::RMFWriteNString(FILE *f, char *str)
{
	unsigned char len;
	
	len = (unsigned char) strlen(str) + 1;
	fwrite(&len,1,1,f);
	fwrite(str,len,1,f);	
}

void GeoMap::RMFWriteString(FILE *f, char *str, int len)
{
	fwrite(str,len,1,f);
}

void GeoMap::RMFWriteColor(FILE *f, GeoColor *color)
{
	fwrite(color,3,1,f);
}

void GeoMap::RMFWriteVector(FILE *f, GeoVector *vector)
{
	struct
	{
		float x, y, z;
	} v;
	
	v.x = vector->x;
	v.y = vector->y;
	v.z = vector->z;
	
	fwrite(&v,12,1,f);
}

void GeoMap::RMFWriteInt(FILE *f, int *pi)
{
	fwrite(pi,4,1,f);
}

void GeoMap::RMFWriteFloat(FILE *f, float *pn)
{
	fwrite(pn,4,1,f);
}

void GeoMap::RMFWriteByte(FILE *f, unsigned char *pb)
{
	fwrite(pb,1,1,f);
}

void GeoMap::RMFWriteInt(FILE *f, int i)
{
	fwrite(&i,4,1,f);
}

void GeoMap::RMFWriteFloat(FILE *f, float n)
{
	fwrite(&n,4,1,f);
}

void GeoMap::RMFWriteByte(FILE *f, unsigned char b)
{
	fwrite(&b,1,1,f);
}

void GeoMap::RMFFill(FILE *f, int d)
{
	while (d-- > 0)
		fwrite("",1,1,f);
}

void GeoMap::RMFWriteVisible(FILE *f, GeoVisible *visible)
{
	RMFWriteInt(f,&visible->visgroup);
	RMFWriteColor(f,&visible->color);
}

void GeoMap::RMFWriteFace(FILE *f, GeoFace *face)
{
	list<GeoEdge>::reverse_iterator rie;
	int i;

	RMFWriteString(f,face->tex.texture,256);
	RMFFill(f,4);
	RMFWriteVector(f,&face->tex.uaxis);
	RMFWriteFloat(f,face->tex.ushift);
	RMFWriteVector(f,&face->tex.vaxis);
	RMFWriteFloat(f,face->tex.vshift);
	RMFWriteFloat(f,face->tex.rot);
	RMFWriteFloat(f,face->tex.uscale);
	RMFWriteFloat(f,face->tex.vscale);
	RMFFill(f,16);

	RMFWriteInt(f,face->edges.size());

	for (rie = face->edges.rbegin(); rie != face->edges.rend(); rie++)
		RMFWriteVector(f,&rie->v1);
	
	for (i = 0,rie = face->edges.rbegin(); i < 3; i++, rie++)
		RMFWriteVector(f,&rie->v1);
}

void GeoMap::RMFWriteSolid(FILE *f, GeoSolid *solid)
{
	list<GeoFace>::iterator iface;

	RMFWriteVisible(f,solid);
	RMFFill(f,4);
	RMFWriteInt(f,solid->faces.size());
	
	for (iface = solid->faces.begin(); iface != solid->faces.end(); iface++)
		RMFWriteFace(f,&*iface);
}

void GeoMap::RMFWriteKey(FILE *f, GeoKey *key)
{
	RMFWriteNString(f,key->name);
	RMFWriteNString(f,key->value);
}

void GeoMap::RMFWriteEntityDef(FILE *f, GeoEntityDef *def)
{
	list<GeoKey>::iterator ikey;
	
	RMFWriteNString(f,def->classname);
	RMFFill(f,4);
	RMFWriteInt(f,&def->flags);
	RMFWriteInt(f,def->keys.size());
	
	for (ikey = def->keys.begin(); ikey != def->keys.end(); ikey++)
		RMFWriteKey(f,&*ikey);
}

void GeoMap::RMFWriteEntity(FILE *f, GeoEntity *entity)
{
	list<GeoSolid>::iterator isolid;
	
	RMFWriteVisible(f,entity);
	RMFWriteInt(f,entity->solids.size());
	
	for (isolid = entity->solids.begin(); isolid != entity->solids.end(); isolid++)
	{
		RMFWriteNString(f,"CMapSolid");
		RMFWriteSolid(f,&*isolid);
	}
	
	RMFWriteEntityDef(f,&entity->def);
	RMFFill(f,14);
	RMFWriteVector(f,&entity->location);
	RMFFill(f,4);
}

void GeoMap::RMFWriteGroup(FILE *f, GeoGroup *group)
{
	list<GeoEntity>::iterator ientity;
	list<GeoSolid>::iterator isolid;
	list<GeoGroup>::iterator igroup;
	
	RMFWriteVisible(f,group);
	RMFWriteInt(f,group->entities.size() + group->solids.size() + group->groups.size());
	
	for (ientity = group->entities.begin(); ientity != group->entities.end(); ientity++)
	{
		RMFWriteNString(f,"CMapEntity");
		RMFWriteEntity(f,&*ientity);
	}
	
	for (isolid = group->solids.begin(); isolid != group->solids.end(); isolid++)
	{
		RMFWriteNString(f,"CMapSolid");
		RMFWriteSolid(f,&*isolid);
	}
	
	for (igroup = group->groups.begin(); igroup != group->groups.end(); igroup++)
	{
		RMFWriteNString(f,"CMapGroup");
		RMFWriteGroup(f,&*igroup);
	}
}

void GeoMap::RMFWriteCorner(FILE *f, GeoCorner *corner)
{
	list<GeoKey>::iterator ikey;

	RMFWriteVector(f,&corner->location);
	RMFWriteInt(f,corner->index);
	RMFWriteString(f,corner->name,128);
	RMFWriteInt(f,corner->keys.size());
	
	for (ikey = corner->keys.begin(); ikey != corner->keys.end(); ikey++)
		RMFWriteKey(f,&*ikey);
}

void GeoMap::RMFWritePath(FILE *f, GeoPath *path)
{
	list<GeoCorner>::iterator icorner;

	RMFWriteString(f,path->name,128);
	RMFWriteString(f,path->classname,128);
	RMFWriteInt(f,path->type);
	RMFWriteInt(f,path->corners.size());
	
	for (icorner = path->corners.begin(); icorner != path->corners.end(); icorner++)
		RMFWriteCorner(f,&*icorner);
}

void GeoMap::RMFWriteVisGroup(FILE *f, GeoVisGroup *vg)
{
	RMFWriteString(f,vg->name,128);
	RMFWriteColor(f,&vg->color);
	RMFFill(f,1);
	RMFWriteInt(f,vg->index);
	RMFWriteByte(f,vg->visible);
	RMFFill(f,3);
}

void GeoMap::RMFWrite(FILE *f)
{
	list<GeoVisGroup>::iterator ivg;
	list<GeoPath>::iterator ipath;

	RMFWriteString(f,"\xCD\xCC\x0C\x40\x52\x4D\x46",7);
	RMFWriteInt(f,visgroups.size());
	
	for (ivg = visgroups.begin(); ivg != visgroups.end(); ivg++)
		RMFWriteVisGroup(f,&*ivg);
	
	RMFWriteNString(f,"CMapWorld");
	RMFWriteGroup(f,this);
	RMFWriteEntityDef(f,&wsdef);
	RMFFill(f,12);

	RMFWriteInt(f,paths.size());
	
	for (ipath = paths.begin(); ipath != paths.end(); ipath++)
		RMFWritePath(f,&*ipath);
}
