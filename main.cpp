/*
 * The contents of this file are copyright 2003 Jedediah Smith
 * <jedediah@silencegreys.com>
 * http://extension.ws/hlfix/
 *
 * This work is licensed under the Creative Commons "Attribution-Share Alike 3.0 Unported" License.
 * To view a copy of this license, visit http://creativecommons.org/licenses/by-sa/3.0/legalcode
 * or, send a letter to Creative Commons, 171 2nd Street, Suite 300, San Francisco, California, 94105, USA.
*/

#include <cstdio>
#include <cctype>
#include "geo.h"
#include "cd.h"

using namespace std;

void ParsePath(const char *pathname, char *path)
{
  const char *p;

  p = pathname;

  while (*p != '\0')
  {
    if (*p == '\\')
    {
      while (pathname != p)
        *(path++) = *(pathname++);

      *(path++) = *(pathname++);
    }

    ++p;
  }

  *path = '\0';
}

int HasPath(const char *pathname)
{
  while (*pathname != '\0')
  {
    if (*pathname == '\\') return 1;
    ++pathname;
  }

  return 0;
}

void ParseFileName(const char *pn, char *fn)
{
  char *p = fn;

  while (*pn != '\0')
    *(p++) = *(pn++);

  *p = '\0';

  while (p != fn)
  {
    if (*(--p) == '.')
    {
      *p = '\0';
      return;
    }
    else if (*p == '\\')
      return;
  }
}

int ParseFloat(float *f, const char *p)
{
  int sign, flag = 0;
  float dec;

  while (*p == ' ' || *p == '\t')
    p++;

  /*
  if (*p == '-')
  {
    sign = 1;
    p++;
  }
  else
    sign = 0;
  */

  *f = 0;

  while (*p >= '0' && *p <= '9')
  {
    flag = 1;
    *f *= 10;
    *f += *p-'0';
    p++;
  }

  if (*p == '.')
  {
    dec = 10;
    p++;

    while (*p >= '0' && *p <= '9')
    {
      flag = 1;
      *f += (*p-'0') / dec;
      dec *= 10;
      p++;
    }
  }

  /*
  if (sign)
    *f = -*f;
  */

  return flag;
}

void Chop(char *str)
{
  while (*str != '\0')
  {
    if (*str == '\x0A' || *str == '\x0D')
    {
      *str = '\0';
      return;
    }

    str++;
  }
}

void strtolower(char *str)
{
  while (*str != '\0')
  {
    *str = tolower(*str);
    ++str;
  }
}

int main(int argc, char **argv)
{
  FILE *fwad, *fout, *frmf;
  GeoMap map;
  float efactor = 1;
  int i, flagWriteRMF, flagWAD, flagTesselate, flagDecompose, flagUnite, flagVisibleOnly;
  char wadfn[FILENAME_MAX+1];
  char outfn[FILENAME_MAX+1];
  char rmffn[FILENAME_MAX+1];
  char option[FILENAME_MAX+1];

  wadfn[0] = outfn[0] = rmffn[0] = '\0';
  FlagGeoDebug = FlagRMFDebug = flagWriteRMF = flagWAD = flagVisibleOnly = 0;
  flagTesselate = flagDecompose = flagUnite = 1;
  map.MAPVersion = 220;

  printf("hlfix v0.9b by Jedediah Smith - http://extension.ws/hlfix/\n");

  try
  {
    for (i = 1; i < argc; i++)
    {
      if (argv[i][0] == '-')
      {
        strncpy(option,argv[i]+1,100);

        if (option[99] != '\0')
          throw "invalid command line option";

        strtolower(option);

        if (strcmp(option,"w") == 0)
        {
               flagWAD = 1;

               if (++i < argc && argv[i][0] != '-')
                  strcpy(wadfn,argv[i]);
               else
                  --i;
        }
        else if (strcmp(option,"o") == 0)
        {
          i++;
          if (argc <= i) throw "missing output filename";
          strcpy(outfn,argv[i]);
        }
        else if (strcmp(option,"e") == 0)
        {
          i++;
          if (argc <= i) throw "missing epsilon factor";
          if (!ParseFloat(&efactor,argv[i]))
            throw "invalid epsilon factor";
        }
        else if (strcmp(option,"m") == 0)
        {
          i++;

          if (argc <= i) throw "missing MAP version";

          if (strcmp(argv[i],"220") == 0)
            map.MAPVersion = 220;
          else if (strcmp(argv[i],"100") == 0)
            map.MAPVersion = 100;
          else throw "invalid MAP version";
        }
        else if (strcmp(option,"r") == 0)
          flagWriteRMF = 1;
        else if (strcmp(option,"v") == 0)
          flagVisibleOnly = 1;
        else if (strcmp(option,"nt") == 0)
          flagTesselate = 0;
        else if (strcmp(option,"nd") == 0)
          flagDecompose = 0;
        else if (strcmp(option,"nu") == 0)
          flagUnite = 0;
        else if (strcmp(option,"na") == 0)
          flagTesselate = flagDecompose = flagUnite = 0;
        else if (strcmp(option,"gd") == 0)
          FlagGeoDebug = 1;
        else if (strcmp(option,"rd") == 0)
          FlagRMFDebug = 1;
        else
          throw "invalid command line option";
      }
      else
        strcpy(rmffn,argv[i]);
    }

    if (rmffn[0] == '\0') throw "you must specify an input file";

    if (strchr(rmffn,'.') == NULL)
      strcat(rmffn,".rmf");

    if (outfn[0] == '\0')
    {
      ParseFileName(rmffn,outfn);
      strcat(outfn,flagWriteRMF ? ".rmf" : ".map");
    }

    if (wadfn[0] == '\0')
    {
      strcpy(wadfn,"wad.txt");
    }

    if (strcmp(rmffn,outfn) == 0)
      throw "input file can't be the same as output file";
  }

  catch (const char *msg)
  {
    printf("Command line error: %s\n",msg);
    printf(
      "Usage: hlfix <mapname>[.rmf] [options]\n"
      "  -o <outfile>           Output file (default is <mapname>.map or <mapname>.rmf)\n"
      "  -w [wadfile]           Use WAD list file (default is wad.txt)\n"
      "  -m <version>           MAP version to output (valid values are 220 or 100, default is 220)"
      "  -r                     Output to RMF file instead of MAP file\n"
      "  -nt                    Don't tesselate non-planar faces\n"
      "  -nd                    Don't decompose non-convex solids\n"
      "  -nu                    Don't unite coplanar faces\n"
      "  -na                    Don't perform ANY geometry correction\n"
      "  -v                     Process and output visible objects only\n"
      "  -e <number>            Epsilon factor for numeric comparisons (default is 1.0)\n");
    return 1;
  }

  GeoEpsilon = (double) efactor * 0.004;

  printf("Using epsilon %lg\n",GeoEpsilon);
  printf("Reading input file %s... ", rmffn);
  fflush(stdout);

  if ((frmf = fopen(rmffn,"rb")) == NULL)
  {
    printf("can't open %s\n",rmffn);
    return 1;
  }

  try
  {
    map.RMFRead(frmf);
    fflush(stdout);
  }

  catch (GeoException *rmfe)
  {
    fclose(frmf);
    printf("error at offest %08xh: %s\n",map.RMFPos,rmfe->msg);
    printf("Entity: %i\n", map.RMFPosEntity);
    printf("Brush: %i\n", map.RMFPosSolid);
    printf("Face: %i\n", map.RMFPosFace);
    printf("Vector: %i\n", map.RMFPosVector);
    printf("Key: %i\n", map.RMFPosKey);
    printf("Path: %i\n", map.RMFPosPath);
    printf("Corner: %i\n", map.RMFPosCorner);
    printf("VisGroup: %i\n", map.RMFPosVisGroup);
    return 1;
  }

  fclose(frmf);

  printf("done\n");
  fflush(stdout);

   if (flagWAD)
   {
    printf("Reading wad list file %s... ",wadfn);
    fflush(stdout);

    if ((fwad = fopen(wadfn,"r")) == NULL)
    {
      printf("can't open %s\n",wadfn);
      return 1;
    }

    while (fgets(wadfn,FILENAME_MAX+1,fwad) != NULL)
    {
      Chop(wadfn);
      map.wads.push_back(string(wadfn));
    }

    fclose(fwad);
    printf("done\n");
  }

  try
  {
    if (flagVisibleOnly)
    {
      printf("Pruning invisible objects\n");
      PruneInvisibleObjects(&map,&map.visgroups);
    }

    printf("Snapping vertices\n");
    SnapVertices(&map);

    if (flagTesselate)
    {
      printf("Tesselating non-planar faces\n");
      TesselateNonPlanarFaces(&map,&map);
    }

    if (flagDecompose)
    {
      printf("Decomposing non-convex solids\n");
      DecomposeGroup(&map);
    }

    if (flagUnite)
    {
      printf("Uniting coplanar faces\n");
      UniteCoplanarFaces(&map);
    }
  }

  catch (GeoException *ex)
  {
    printf("  ERROR (Entity %i, Brush %i): %s\n",ex->entity, ex->brush, ex->msg);
    delete ex;
  }

  printf("Writing output file %s... ",outfn);
  fflush(stdout);

  if ((fout = fopen(outfn,flagWriteRMF?"wb":"w")) == NULL)
  {
    printf("can't open %s\n",outfn);
    return 1;
  }

  try
  {
    if (flagWriteRMF)
      map.RMFWrite(fout);
    else
      map.MAPWrite(fout);
  }

  catch (GeoException *ex)
  {
    printf("  ERROR (Entity %i, Brush %i): %s\n",ex->entity, ex->brush, ex->msg);
    delete ex;
  }

  fclose(fout);

  printf("done\n");

  return 0;
}

