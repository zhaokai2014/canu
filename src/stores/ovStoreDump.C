
/******************************************************************************
 *
 *  This file is part of canu, a software program that assembles whole-genome
 *  sequencing reads into contigs.
 *
 *  This software is based on:
 *    'Celera Assembler' (http://wgs-assembler.sourceforge.net)
 *    the 'kmer package' (http://kmer.sourceforge.net)
 *  both originally distributed by Applera Corporation under the GNU General
 *  Public License, version 2.
 *
 *  Canu branched from Celera Assembler at its revision 4587.
 *  Canu branched from the kmer project at its revision 1994.
 *
 *  This file is derived from:
 *
 *    src/AS_OVS/overlapStore.C
 *    src/AS_OVS/overlapStore.c
 *
 *  Modifications by:
 *
 *    Brian P. Walenz from 2007-MAR-08 to 2013-AUG-01
 *      are Copyright 2007-2013 J. Craig Venter Institute, and
 *      are subject to the GNU General Public License version 2
 *
 *    Sergey Koren from 2008-DEC-16 to 2009-AUG-14
 *      are Copyright 2008-2009 J. Craig Venter Institute, and
 *      are subject to the GNU General Public License version 2
 *
 *    Gregory Sims from 2012-FEB-01 to 2012-MAR-14
 *      are Copyright 2012 J. Craig Venter Institute, and
 *      are subject to the GNU General Public License version 2
 *
 *    Brian P. Walenz from 2014-AUG-22 to 2015-JUN-25
 *      are Copyright 2014-2015 Battelle National Biodefense Institute, and
 *      are subject to the BSD 3-Clause License
 *
 *    Brian P. Walenz beginning on 2015-OCT-21
 *      are a 'United States Government Work', and
 *      are released in the public domain
 *
 *    Sergey Koren beginning on 2016-MAR-11
 *      are a 'United States Government Work', and
 *      are released in the public domain
 *
 *  File 'README.licenses' in the root directory of this distribution contains
 *  full conditions and disclaimers for each license.
 */

#include "AS_global.H"
#include "strings.H"

#include "sqStore.H"
#include "ovStore.H"
#include "tgStore.H"

#include <algorithm>
using namespace std;



class bogartStatus {
public:
  uint32  best5id;
  uint32  best3id;

  uint32  tigId;

  uint32  isContained  : 1;
  uint32  isIgnored    : 1;
  uint32  isCovGap     : 1;
  uint32  isLopsided   : 1;
  uint32  isSpur       : 1;
};



class dumpParameters {
public:
  dumpParameters() {
    no5p               = false;
    no3p               = false;
    noContainer        = false;
    noContained        = false;
    noRedundant        = false;

    noBogartContained  = false;
    noBogartSuspicious = false;
    noBogartSpur       = false;

    erateMin           = 0.0;
    erateMax           = 1.0;

    lengthMin          = 0;
    lengthMax          = UINT32_MAX;

    queryMin           = 0;
    queryMax           = UINT32_MAX;

    withStatus         = false;
    withTigID          = false;

    status             = NULL;


    ovlKept            = 0;
    ovlFiltered        = 0;

    ovl5p              = 0;
    ovl3p              = 0;
    ovlContainer       = 0;
    ovlContained       = 0;
    ovlRedundant       = 0;

    ovlErateLo         = 0;
    ovlErateHi         = 0;

    ovlLengthLo        = 0;
    ovlLengthHi        = 0;
  };

  ~dumpParameters() {
    delete [] status;
  };


  bool        parametersAreDefaults(void) {       //  Returns true if all the parameters
    return((no5p               == false) &&       //  are the default setting.
           (no3p               == false) &&       //
           (noContainer        == false) &&       //  This looks horrid, but it was easier
           (noContained        == false) &&       //  and simpler than tracking what
           (noRedundant        == false) &&       //  was set.  I'd need to make setter
           (noBogartContained  == false) &&       //  functions for everything, make the
           (noBogartSuspicious == false) &&       //  setter set a 'isChanged' flag, and
           (noBogartSpur       == false) &&       //  then update command line parsing.
           (erateMin           == 0.0) &&         //
           (erateMax           == 1.0) &&         //  It took longer to write this comment
           (lengthMin          == 0) &&           //  than to write this function.
           (lengthMax          == UINT32_MAX) &&  //
           (queryMin           == 0) &&           //  So there.
           (queryMax           == UINT32_MAX));
  };

  void        loadBogartStatus(const char *prefix, uint32 nReads);
  void        loadBogartTigs(const char *tigname);

  void        drawPicture(uint32         Aid,
                          ovOverlap     *overlaps,
                          uint64         overlapsLen,
                          sqStore       *seqStore,
                          bool           withScores);


  bool        filterOverlap(ovOverlap *overlap) {
    double erate    = overlap->erate();
    uint32 length   = overlap->length();
    int32  ahang    = overlap->a_hang();
    int32  bhang    = overlap->b_hang();
    bool   filtered = false;

    if ((no5p == true) && (ahang < 0) && (bhang < 0)) {
      ovl5p++;
      filtered = true;
    }

    if ((no3p == true) && (ahang > 0) && (bhang > 0)) {
      ovl3p++;
      filtered = true;
    }

    if ((noContainer) && (ahang <= 0) && (bhang >= 0)) {
      ovlContainer++;
      filtered = true;
    }

    if ((noContained) && (ahang >= 0) && (bhang <= 0)) {
      ovlContained++;
      filtered = true;
    }

    if ((noRedundant) && (overlap->a_iid >= overlap->b_iid)) {
      ovlRedundant++;
      filtered = true;
    }

    if ((noBogartContained) &&
        (withStatus) &&
        ((status[overlap->a_iid].isContained == true) ||
         (status[overlap->b_iid].isContained == true))) {
      filtered = true;
    }

    if ((noBogartSuspicious) &&
        (withStatus) &&
        ((status[overlap->a_iid].isCovGap == true) ||
         (status[overlap->b_iid].isCovGap == true))) {
      filtered = true;
    }

    if ((noBogartSuspicious) &&
        (withStatus) &&
        ((status[overlap->a_iid].isLopsided == true) ||
         (status[overlap->b_iid].isLopsided == true))) {
      filtered = true;
    }

    if ((noBogartSpur) &&
        (withStatus) &&
        ((status[overlap->a_iid].isSpur == true) ||
         (status[overlap->b_iid].isSpur == true))) {
      filtered = true;
    }

    if ((overlap->b_iid < queryMin) ||
        (queryMax < overlap->b_iid)) {
      filtered = true;
    }

    if (erate < erateMin) {
      ovlErateLo++;
      filtered = true;
    }

    if (erate > erateMax) {
      ovlErateHi++;
      filtered = true;
    }

    if (length < lengthMin) {
      ovlLengthLo++;
      filtered = true;
    }

    if (length > lengthMax) {
      ovlLengthHi++;
      filtered = true;
    }

    return(filtered);
  };

  //  Filtering options.

  bool           no5p;
  bool           no3p;
  bool           noContainer;
  bool           noContained;
  bool           noRedundant;

  bool           noBogartContained;
  bool           noBogartSuspicious;
  bool           noBogartSpur;

  uint32         queryMin;
  uint32         queryMax;

  double         erateMin;
  double         erateMax;

  uint32         lengthMin;
  uint32         lengthMax;

  bool           withStatus;
  bool           withTigID;

  bogartStatus  *status;

  //  Counts of what we filtered.

  uint64         ovlKept;
  uint64         ovlFiltered;

  uint64         ovl5p;
  uint64         ovl3p;
  uint64         ovlContainer;
  uint64         ovlContained;
  uint64         ovlRedundant;

  uint64         ovlErateHi;
  uint64         ovlErateLo;

  uint64         ovlLengthHi;
  uint64         ovlLengthLo;
};



void
dumpParameters::loadBogartStatus(const char *prefix, uint32 nReads) {
  FILE         *E    = NULL;

  if (prefix == NULL)
    return;

  withStatus = true;

  if (fileExists(prefix) == true)
    E = AS_UTL_openInputFile(prefix);

  if (fileExists(prefix, '.', "edges") == true)
    E = AS_UTL_openInputFile(prefix, '.', "edges");

  if (fileExists(prefix, '.', "best.edges") == true)
    E = AS_UTL_openInputFile(prefix, '.', "best.edges");

  if (E == NULL) {
    fprintf(stderr, "Failed to find bogart best edges in either '%s', '%s.edges', or '%s.best.edges'.\n",
            prefix, prefix, prefix);
    exit(1);
  }


  allocateArray(status, nReads+1, resizeArray_clearNew);


  uint32        Llen = 0;
  uint32        Lmax = 0;
  char         *L    = NULL;
  splitToWords  W;

  while (AS_UTL_readLine(L, Llen, Lmax, E)) {
    W.split(L);

    if (W.numWords() == 0)
      continue;

    uint32  rid = W.touint32(0);

    uint32  p5 = 3;
    uint32  p3 = 8;

    if ((W[5][0] != 'M') &&   //  The 'M' mutual best flag is optional in columne 5.
        (W[5][0] != '-'))     //  If there is a space there, decrement the
      p3--;                   //   location of the 3' id.

    status[rid].best5id     = W.touint32(p5);
    status[rid].best3id     = W.touint32(p3);

    status[rid].tigId       = UINT32_MAX;

    status[rid].isContained = (W[2][0] == 'C');
    status[rid].isIgnored   = (W[2][1] == 'I');
    status[rid].isCovGap    = (W[2][2] == 'G');
    status[rid].isLopsided  = (W[2][3] == 'L');
    status[rid].isSpur      = (W[2][4] == 'S');
  }

  AS_UTL_closeFile(E);

  delete [] L;
}



void
dumpParameters::loadBogartTigs(const char *tigpath) {

  if (directoryExists(tigpath) == false)
    return;

  withTigID = true;

  fprintf(stderr, "loading read to tig mapping from '%s'\n", tigpath);

  tgStore  *tigs = new tgStore(tigpath, 1);

  for (uint32 tt=0; tt<tigs->numTigs(); tt++) {
    tgTig *tig = tigs->loadTig(tt);

    if (tig != NULL) {
      for (uint32 ii=0; ii<tig->numberOfChildren(); ii++)
        status[ tig->getChild(ii)->ident() ].tigId = tt;
    }

    tigs->unloadTig(tt);
  }
  delete tigs;
}



class sortByPosition {
public:
  bool operator()(const ovOverlap &a, const ovOverlap &b) {
    if (a.a_bgn() < b.a_bgn())  return(true);
    if (a.a_bgn() > b.a_bgn())  return(false);

    return(a.a_end() < b.a_end());
  };
};



void
dumpParameters::drawPicture(uint32         Aid,
                            ovOverlap     *overlaps,
                            uint64         overlapsLen,
                            sqStore       *seqStore,
                            bool           withScores) {
  char     line[256] = {0};

  uint32   MHS   = 9;  //  Max Hang Size, amount of padding for "+### "

  uint32    Alen = seqStore->sqStore_getReadLength(Aid);

  if (overlapsLen == 0)
    return;

  for (int32 i=0; i<256; i++)
    line[i] = ' ';

  for (int32 i=0; i<100; i++)
    line[i + MHS] = '-';

  line[MHS]            = '|';
  line[MHS + 1 +  99] = '>';
  line[MHS + 1 + 100] = '|';
  line[MHS + 1 + 101] = 0;

  //  Draw the read we're showing overlaps for.
  //  Annotate it as either 'contained' or 'suspicious' as needed.

  fprintf(stdout, "A %7d:%-7d A %9d %7d:%-7d %7d          %s\n",
          0, Alen,
          Aid,
          0, Alen, Alen,
          line);

  sort(overlaps, overlaps + overlapsLen, sortByPosition());

  //  Build ascii representations for each overlapping read.

  for (uint32 o=0; o<overlapsLen; o++) {
    uint32    Bid  = overlaps[o].b_iid;

    //  Find bgn/end points on each read.  If the overlap is reverse complement,
    //  the B coords are flipped so that bgn > end.

    uint32   ovlBgnA = overlaps[o].a_bgn();
    uint32   ovlEndA = overlaps[o].a_end();

    uint32   ovlBgnB = overlaps[o].b_bgn();
    uint32   ovlEndB = overlaps[o].b_end();

    //  For the A read, find the points in our string representation where the overlap ends.

    uint32 ovlStrBgn = (int32)floor(ovlBgnA * 100.0 / Alen + MHS + 1);
    uint32 ovlStrEnd = (int32)ceil (ovlEndA * 100.0 / Alen + MHS + 1);

    //  For the B read, find how much is unaliged on each end.  Though the
    //  store directly keeps this information, we can't get to it, and have
    //  to reverse the compuitation.

    uint32  ovlBgnHang = overlaps[o].dat.ovl.bhg5;
    uint32  ovlEndHang = overlaps[o].dat.ovl.bhg3;

    //  Just some checking on the overlap.

    assert(ovlBgnA < ovlEndA);  //  The A coordiantes are always forward

    if (overlaps[o].flipped() == false)
      assert(ovlBgnB < ovlEndB);  //  Forward overlaps are forward
    else
      assert(ovlEndB < ovlBgnB);  //  Flipped overlaps are reversed

    assert(ovlStrBgn >= MHS + 1);
    assert(ovlStrEnd <= MHS + 1 + 100);


    //
    //  DRAW!
    //


    //  Clear the string and add borders for the source read.

    for (int32 i=0; i<256; i++)
      line[i] = ' ';

    line[MHS]               = '|';
    line[MHS + 1 + 100]     = '|';

    //  Write in the unaligned sequence at the 5' end.

    if (ovlBgnHang > 0) {
      char  str[256];
      int32 len = snprintf(str, 256, "+%d", ovlBgnHang);

      for (int32 i=0; i<len; i++)
        line[ovlStrBgn - len - 2 + i] = str[i];
    }

    //  Draw the basic overlap.  Use light lines '-' for non-best overlaps, and '=' for best overlaps.

    bool  isBest  = ((withStatus) && (((status[Aid].best5id == Bid) && (overlaps[o].overlapAEndIs5prime() == true)) ||
                                      ((status[Aid].best3id == Bid) && (overlaps[o].overlapAEndIs3prime() == true))));

#define NARROW_CHARS
#ifdef NARROW_CHARS
    char   c = '-';

    if ((withStatus) && (status[Bid].isContained == true))   c = 'c';   //  Draw special reads specially.
    if ((withStatus) && (status[Bid].isIgnored   == true))   c = 'i';
    if ((withStatus) && (status[Bid].isCovGap    == true))   c = 'g';
    if ((withStatus) && (status[Bid].isLopsided  == true))   c = 'l';
    if ((withStatus) && (status[Bid].isSpur      == true))   c = 's';

    if (isBest == true)                                      c = '=';   //  But always draw best as best.

    for (uint32 i=ovlStrBgn; i<ovlStrEnd; i++)
      line[i] = c;

    if (overlaps[o].flipped() == true)
      line[ovlStrBgn] = '<';
    else
      line[ovlStrEnd-1] = '>';

#else

    for (uint32 i=ovlStrBgn; i<ovlStrEnd; i++)
      line[i] = isBestC;

    if (overlaps[o].flipped() == true)
      line[ovlStrBgn] = '<';
    else
      line[ovlStrEnd-1] = '>';

#endif



    assert(line[ovlStrBgn]   != ' ');
    assert(line[ovlStrEnd-1] != ' ');

    //  Write in the unaligned sequence at the 3' end.  The NUL from the borders
    //  is overwritten, and a new one added at the end of our string.

    if (ovlEndHang > 0) {
      char  str[256];
      int32 len = snprintf(str, 256, " +%d", ovlEndHang);

      if (ovlStrEnd == MHS + 1 + 100) {
        for (int32 i=0; i<len; i++)
          line[ovlStrEnd + 1 + i] = str[i];
      }

      else {
        for (int32 i=0; i<len; i++)
          line[ovlStrEnd + 2 + i] = str[i];
      }
    }

    //  Write in any annotation(s)

    uint32 annotation = MHS + 1+ 100 + 1 + MHS + 1;

    if (withStatus) {                                                                  //  "1234567890123456"
      if      (status[Bid].isContained == true)   strncpy(line + annotation, (const char *)"contained   ", 12);
      else if (status[Bid].isIgnored   == true)   strncpy(line + annotation, (const char *)"ignored     ", 12);
      else if (status[Bid].isCovGap    == true)   strncpy(line + annotation, (const char *)"coverage-gap", 12);
      else if (status[Bid].isLopsided  == true)   strncpy(line + annotation, (const char *)"lopsided    ", 12);
      else if (status[Bid].isSpur      == true)   strncpy(line + annotation, (const char *)"spur        ", 12);
      else                                        strncpy(line + annotation, (const char *)"dovetail    ", 12);

      annotation += 13;
    }

    //  Write in any tigID

    if (withTigID) {
      char    annoStr[16];
      uint32  annoLen = 0;

      if (status[Bid].tigId < UINT32_MAX)
        annoLen = sprintf(annoStr, "tig=%u", status[Bid].tigId);
      else
        annoLen = sprintf(annoStr, "tig=---");

      for (uint32 ii=0; ii<annoLen; ii++)
        line[annotation + ii] = annoStr[ii];

      annotation += 13;
    }

    //  Write in any overlap score.

    if (withScores) {
      char    annoStr[16];
      uint32  annoLen = sprintf(annoStr, "score=%u", overlaps[o].overlapScore());

      for (uint32 ii=0; ii<annoLen; ii++)
        line[annotation + ii] = annoStr[ii];

      annotation += 13;
    }

    //  Search backwards for the first non-space, and make it the end of string.

    for (int32 ii=254; ((ii >= 0) && (line[ii] == ' ')); ii--)
      line[ii] = 0;

    //  Report!

    fprintf(stdout, "A %7d:%-7d B %9d %7d:%-7d %7d  %5.2f%%  %s\n",
            ovlBgnA,
            ovlEndA,
            Bid,
            min(ovlBgnB, ovlEndB),
            max(ovlBgnB, ovlEndB),
            seqStore->sqStore_getReadLength(Bid),
            overlaps[o].erate() * 100.0,
            line);
  }
}



int
main(int argc, char **argv) {
  char                 *seqName     = NULL;
  char                 *ovlName     = NULL;
  char                 *outPrefix   = NULL;
  char                 *bogartPath  = NULL;
  char                 *bogTigPath  = NULL;

  dumpParameters        params;

  char                  ovlString[1024];

  bool                  asOverlaps  = true;    //  What to show?
  bool                  asPicture   = false;
  bool                  asMetadata  = false;
  bool                  asCounts    = false;
  bool                  asErateLen  = false;

  bool                  asCoords    = true;       //  How to show overlaps?
  bool                  asHangs     = false;
  bool                  asUnaligned = false;
  bool                  asPAF       = false;
  bool                  asGFA       = false;
  bool                  asBinary    = false;
  bool                  withScores  = true;

  uint32                bgnID       = 1;
  uint32                endID       = UINT32_MAX;

  argc = AS_configure(argc, argv);

  vector<char *>  err;
  int             arg=1;
  while (arg < argc) {
    if      (strcmp(argv[arg], "-S") == 0)
      seqName = argv[++arg];

    else if (strcmp(argv[arg], "-O") == 0)
      ovlName = argv[++arg];


    else if (strcmp(argv[arg], "-overlaps") == 0) {
      asOverlaps = true;
      asPicture  = false;
      asMetadata = false;
      asCounts   = false;
      asErateLen = false;
      if ((arg+1 < argc) && (argv[arg+1][0] != '-'))
        decodeRange(argv[++arg], bgnID, endID);
    }

    else if ((strcmp(argv[arg], "-picture") == 0) ||
             (strcmp(argv[arg], "-p")       == 0)) {
      asOverlaps = false;
      asPicture  = true;
      asMetadata = false;
      asCounts   = false;
      asErateLen = false;
      if ((arg+1 < argc) && (argv[arg+1][0] != '-'))
        decodeRange(argv[++arg], bgnID, endID);
    }

    else if (strcmp(argv[arg], "-metadata") == 0) {
      asOverlaps = false;
      asPicture  = false;
      asMetadata = true;
      asCounts   = false;
      asErateLen = false;
      if ((arg+1 < argc) && (argv[arg+1][0] != '-'))
        decodeRange(argv[++arg], bgnID, endID);
    }

    else if (strcmp(argv[arg], "-counts") == 0) {
      asOverlaps = false;
      asPicture  = false;
      asMetadata = false;
      asCounts   = true;
      asErateLen = false;
      if ((arg+1 < argc) && (argv[arg+1][0] != '-'))        //  NOT CORRECT
        decodeRange(argv[++arg], bgnID, endID);
    }

    else if (strcmp(argv[arg], "-eratelen") == 0) {
      asOverlaps = false;
      asPicture  = false;
      asMetadata = false;
      asCounts   = false;
      asErateLen = true;
      if ((arg+1 < argc) && (argv[arg+1][0] != '-'))        //  NOT CORRECT
        decodeRange(argv[++arg], bgnID, endID);
    }


    else if (strcmp(argv[arg], "-prefix") == 0)
      outPrefix = argv[++arg];


    else if (strcmp(argv[arg], "-raw") == 0)
      sqRead_setDefaultVersion(sqRead_raw);

    else if (strcmp(argv[arg], "-obt") == 0)
      sqRead_setDefaultVersion(sqRead_corrected);

    else if (strcmp(argv[arg], "-utg") == 0)
      sqRead_setDefaultVersion(sqRead_trimmed);


    else if (strcmp(argv[arg], "-coords") == 0) {
      asCoords    = true;
      asHangs     = false;
      asUnaligned = false;
      asPAF       = false;
      asGFA       = false;
      asBinary    = false;
    }

    else if (strcmp(argv[arg], "-hangs") == 0) {
      asCoords    = false;
      asHangs     = true;
      asUnaligned = false;
      asPAF       = false;
      asGFA       = false;
      asBinary    = false;
    }

    else if (strcmp(argv[arg], "-unaligned") == 0) {
      asCoords    = false;
      asHangs     = false;
      asUnaligned = true;
      asPAF       = false;
      asGFA       = false;
      asBinary    = false;
    }

    else if (strcmp(argv[arg], "-paf") == 0) {
      asCoords    = false;
      asHangs     = false;
      asUnaligned = false;
      asPAF       = true;
      asGFA       = false;
      asBinary    = false;
    }

    else if (strcmp(argv[arg], "-gfa") == 0) {
      asCoords    = false;
      asHangs     = false;
      asUnaligned = false;
      asPAF       = false;
      asGFA       = true;
      asBinary    = false;
    }

    else if (strcmp(argv[arg], "-binary") == 0) {
      asCoords    = false;
      asHangs     = false;
      asUnaligned = false;
      asPAF       = false;
      asGFA       = false;
      asBinary    = true;
    }


    else if (strcmp(argv[arg], "-no5p") == 0)
      params.no5p = true;

    else if (strcmp(argv[arg], "-no3p") == 0)
      params.no3p = true;

    else if (strcmp(argv[arg], "-nocontainer") == 0)
      params.noContainer = true;

    else if (strcmp(argv[arg], "-nocontained") == 0)
      params.noContained = true;

    else if (strcmp(argv[arg], "-noredundant") == 0)
      params.noRedundant = true;

    else if (strcmp(argv[arg], "-query") == 0)
      decodeRange(argv[++arg], params.queryMin, params.queryMax);

    else if (strcmp(argv[arg], "-erate") == 0)
      decodeRange(argv[++arg], params.erateMin, params.erateMax);

    else if (strcmp(argv[arg], "-length") == 0)
      decodeRange(argv[++arg], params.lengthMin, params.lengthMax);

    else if (strcmp(argv[arg], "-bogart") == 0)
      bogartPath = argv[++arg];
    else if (strcmp(argv[arg], "-bogarttigs") == 0)
      bogTigPath = argv[++arg];

    else if (strcmp(argv[arg], "-nobogartcontained") == 0)
      params.noBogartContained = true;
    else if (strcmp(argv[arg], "-nobogartsuspicious") == 0)
      params.noBogartSuspicious = true;
    else if (strcmp(argv[arg], "-nobogartspur") == 0)
      params.noBogartSpur = true;

    else {
      char *s = new char [1024];
      snprintf(s, 1024, "%s: unknown option '%s'.\n", argv[0], argv[arg]);
      err.push_back(s);
    }

    arg++;
  }

  if (seqName == NULL)
    err.push_back("ERROR: no input seqStore (-S) supplied.\n");

  if (ovlName == NULL)
    err.push_back("ERROR: no input ovlStore (-O) supplied.\n");

  if ((asBinary) && (outPrefix == NULL))
    err.push_back("ERROR: -prefix is necessary for -binary output.\n");

  if (err.size() > 0) {
    fprintf(stderr, "usage: %s -S seqStore -O ovlStore ...\n", argv[0]);
    fprintf(stderr, "  -S seqStore         mandatory path to a sequence store\n");
    fprintf(stderr, "  -O ovlStore         mandatory path to an overlap store\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "WHAT TO DUMP:\n");
    fprintf(stderr, "  Select what data to dump.  All take an optional read ID, or inclusive\n");
    fprintf(stderr, "  range of read IDs, to dump.  Dumps are to stdout.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  -overlaps [b[-e]]   dump overlaps (default)\n");
    fprintf(stderr, "  -picture  [b[-e]]   dump an ASCII picture of the overlaps relative to a read\n");
    fprintf(stderr, "  -metadata [b[-3]]   tabular metadata, including the number of overlaps per read\n");
    fprintf(stderr, "  -counts   [b[-e]]   the number of overlaps per read\n");
    fprintf(stderr, "  -eratelen [b[-e]]   a histogram of overlap length vs error rate\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  -prefix name        * for -eratelen, write histogram to name.dat\n");
    fprintf(stderr, "                        and also output a gnuplot script to name.gp\n");
    fprintf(stderr, "                      * for -binary, mandatory, write overlaps to name.ovb\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "WHICH READ VERSION TO USE:\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  -raw                uncorrected raw reads\n");
    fprintf(stderr, "  -obt                corrected reads\n");
    fprintf(stderr, "  -utg                trimmed reads\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "FORMAT OF -overlaps OUTPUT\n");
    fprintf(stderr, "  NOTE!  Overlap type flags are only reported with -unaligned.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  -coords             as coordinates on each read (default)\n");
    fprintf(stderr, "  -hangs              as dovetail hangs\n");
    fprintf(stderr, "  -unaligned          as unaligned regions on each read\n");
    fprintf(stderr, "  -paf                as miniasm Pairwise mApping Format\n");
    fprintf(stderr, "  -gfa                as Graphical Fragment Assembly format\n");
    fprintf(stderr, "  -binary             as an overlapper output file (needs -prefix)\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "OVERLAP FILTERING\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  -no5p               do not show oevrlaps off the 5' end of the A read\n");
    fprintf(stderr, "  -no3p               do not show overlaps off the 3' end of the A read\n");
    fprintf(stderr, "  -nocontainer        do not show overlaps that contain some other B read\n");
    fprintf(stderr, "  -nocontained        do not show overlaps that are contained in some other B read\n");
    fprintf(stderr, "  -noreduntant        do not show overlaps where the A read ID is more than the B read ID\n");
    fprintf(stderr, "  -query a[-b]        display only overlaps that are to these other B read IDs\n");
    fprintf(stderr, "  -erate f            display only overlaps less than f fraction error\n");
    fprintf(stderr, "  -length min[-max]   display only overlaps between min and max bases long\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  -bogart asm.best    annotate a picture with labels from bogart asm.best.edges output\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  -nobogartcontained  do not show overlaps involving contained reads\n");
    fprintf(stderr, "  -nobogartsuspicious do not show overlaps involving suspicious reads (coverage gap or lopsided edges)\n");
    fprintf(stderr, "  -nobogartspur       do not show iverlaps involving spur reads\n");
    fprintf(stderr, "\n");

    for (uint32 ii=0; ii<err.size(); ii++)
      if (err[ii])
        fputs(err[ii], stderr);

    exit(1);
  }

  //
  //  Open stores, allocate space to store overlaps.
  //

  sqStore   *seqStore = new sqStore(seqName);
  ovStore   *ovlStore = new ovStore(ovlName, seqStore);

  uint32     ovlLen   = 0;
  uint32     ovlMax   = 65536;
  ovOverlap *ovl      = new ovOverlap [ovlMax];

  //
  //  Fix up ranges and restrict the overlaps.
  //

  if (endID > seqStore->sqStore_lastReadID())
    endID = seqStore->sqStore_lastReadID();

  if (endID < bgnID)
    fprintf(stderr, "ERROR: invalid bgn/end range bgn=%u end=%u; only %u reads in the store\n", bgnID, endID, seqStore->sqStore_lastReadID()), exit(1);

  ovlStore->setRange(bgnID, endID);

  //
  //  Load bogart status.
  //

  params.loadBogartStatus(bogartPath, seqStore->sqStore_lastReadID());
  params.loadBogartTigs(bogTigPath);

  //
  //  If dumping metadata, no filtering is needed, just tell the store to dump.
  //

  if (asMetadata) {
    ovlStore->dumpMetaData(bgnID, endID);
  }

  //
  //  If dumping as counts, maybe we need to filter, maybe not.
  //

  if ((asCounts) && (params.parametersAreDefaults() == true)) {
    uint32   *nopr = ovlStore->numOverlapsPerRead();

    for (uint32 ii=bgnID; ii<=endID; ii++)
      fprintf(stdout, "%u\t%u\n", ii, nopr[ii]);

    delete [] nopr;
  }

  if ((asCounts) && (params.parametersAreDefaults() == false)) {
    uint32   *nopr = new uint32 [seqStore->sqStore_lastReadID() + 1];

    ovlLen = ovlStore->loadBlockOfOverlaps(ovl, ovlMax);

    while (ovlLen > 0) {
      for (uint32 oo=0; oo<ovlLen; oo++) {
        if (params.filterOverlap(ovl + oo) == true)
          continue;

        nopr[ovl[oo].a_iid]++;
        //nopr[ovl[oo].b_iid]++;
      }

      ovlLen = ovlStore->loadBlockOfOverlaps(ovl, ovlMax);
    }

    for (uint32 ii=bgnID; ii<=endID; ii++)
      fprintf(stdout, "%u\t%u\n", ii, nopr[ii]);

    delete [] nopr;
  }

  //
  //  If as erate-v-length, again, maybe, maybe not.  This must ALWAYS be
  //  recomputed.  Prior to Dec 2019 the store saved a copy of the histogram
  //  in the metadata, but we realized that the histogram is no longer valid
  //  -- and impossible to update -- after OEA runs.  So it was removed.
  //

  if (asErateLen) {
    ovErateLengthHistogram *hist = new ovErateLengthHistogram(seqStore);

    ovlLen = ovlStore->loadBlockOfOverlaps(ovl, ovlMax);

    while (ovlLen > 0) {
      for (uint32 oo=0; oo<ovlLen; oo++) {
        if (params.filterOverlap(ovl + oo) == true)
          continue;

        hist->addOverlap(ovl + oo);
      }

      ovlLen = ovlStore->loadBlockOfOverlaps(ovl, ovlMax);
    }

    //  If no outPrefix, dump the histogram to stdout.
    //  Otherwise, dump to a file and emit a gnuplot script.

    if (outPrefix == NULL) {
      hist->dumpEvalueLength(stdout);
    }

    else {

      FILE *D = AS_UTL_openOutputFile(outPrefix, '.', "dat");
      hist->dumpEvalueLength(D);
      AS_UTL_closeFile(D, outPrefix, '.', "dat");

      FILE *G = AS_UTL_openOutputFile(outPrefix, '.', "gp");
      fprintf(G, "unset key\n");
      fprintf(G, "set tic scale 0\n");
      fprintf(G, "\n");
      fprintf(G, "set palette defined(\\\n");
      fprintf(G, "0       0.2314  0.2980  0.7529,\\\n");
      fprintf(G, "0.03125 0.2667  0.3529  0.8000,\\\n");
      fprintf(G, "0.0625  0.3020  0.4078  0.8431,\\\n");
      fprintf(G, "0.09375 0.3412  0.4588  0.8824,\\\n");
      fprintf(G, "0.125   0.3843  0.5098  0.9176,\\\n");
      fprintf(G, "0.15625 0.4235  0.5569  0.9451,\\\n");
      fprintf(G, "0.1875  0.4667  0.6039  0.9686,\\\n");
      fprintf(G, "0.21875 0.5098  0.6471  0.9843,\\\n");
      fprintf(G, "0.25    0.5529  0.6902  0.9961,\\\n");
      fprintf(G, "0.28125 0.5961  0.7255  1.0000,\\\n");
      fprintf(G, "0.3125  0.6392  0.7608  1.0000,\\\n");
      fprintf(G, "0.34375 0.6824  0.7882  0.9922,\\\n");
      fprintf(G, "0.375   0.7216  0.8157  0.9765,\\\n");
      fprintf(G, "0.40625 0.7608  0.8353  0.9569,\\\n");
      fprintf(G, "0.4375  0.8000  0.8510  0.9333,\\\n");
      fprintf(G, "0.46875 0.8353  0.8588  0.9020,\\\n");
      fprintf(G, "0.5     0.8667  0.8667  0.8667,\\\n");
      fprintf(G, "0.53125 0.8980  0.8471  0.8196,\\\n");
      fprintf(G, "0.5625  0.9255  0.8275  0.7725,\\\n");
      fprintf(G, "0.59375 0.9451  0.8000  0.7255,\\\n");
      fprintf(G, "0.625   0.9608  0.7686  0.6784,\\\n");
      fprintf(G, "0.65625 0.9686  0.7333  0.6275,\\\n");
      fprintf(G, "0.6875  0.9686  0.6941  0.5804,\\\n");
      fprintf(G, "0.71875 0.9686  0.6510  0.5294,\\\n");
      fprintf(G, "0.75    0.9569  0.6039  0.4824,\\\n");
      fprintf(G, "0.78125 0.9451  0.5529  0.4353,\\\n");
      fprintf(G, "0.8125  0.9255  0.4980  0.3882,\\\n");
      fprintf(G, "0.84375 0.8980  0.4392  0.3451,\\\n");
      fprintf(G, "0.875   0.8706  0.3765  0.3020,\\\n");
      fprintf(G, "0.90625 0.8353  0.3137  0.2588,\\\n");
      fprintf(G, "0.9375  0.7961  0.2431  0.2196,\\\n");
      fprintf(G, "0.96875 0.7529  0.1569  0.1843,\\\n");
      fprintf(G, "1       0.7059  0.0157  0.1490\\\n");
      fprintf(G, ")\n");
      fprintf(G, "\n");
      fprintf(G, "set format cb '%%5.0f'\n");
      fprintf(G, "set colorbox user size .03, .6 noborder\n");
      fprintf(G, "set cbtics scale 0\n");
      fprintf(G, "set cblabel 'NumOlaps'\n");
      fprintf(G, "\n");
      fprintf(G, "set xlabel 'Overlap Length'\n");
      fprintf(G, "set ylabel 'Error Rate'\n");
      fprintf(G, "\n");
      fprintf(G, "set view map\n");
      fprintf(G, "\n");
      fprintf(G, "set terminal 'png' size 1024,1024\n");
      fprintf(G, "set output '%s.png'\n", outPrefix);
      fprintf(G, "\n");
      fprintf(G, "plot [0:" F_U32 "] [0:%.3f] '%s.dat' using 1:2:3 with image\n",
              hist->maxLength(),
              hist->maxErate(),
              outPrefix);

      AS_UTL_closeFile(G, outPrefix, '.', "gp");
    }

    //  All dumped!  Delete the data.

    delete hist;
  }


  //
  //  But if dumping actual overlaps, we've got to filter, and
  //  change the output format willy nilly.
  //

  if (asOverlaps) {
    ovFile  *binaryFile = NULL;

    uint32  *gfaReads  = NULL;
    FILE    *gfaLinks  = NULL;
    FILE    *gfaFile   = NULL;

    if (asGFA) {
      allocateArray(gfaReads, seqStore->sqStore_lastReadID() + 1);

      gfaLinks  = AS_UTL_openOutputFile(outPrefix, '.', "gfalinks");
      gfaFile   = AS_UTL_openOutputFile(outPrefix, '.', "gfa");
    }

    if (asBinary) {
      char     binaryName[FILENAME_MAX + 1];

      snprintf(binaryName, FILENAME_MAX, "%s.ovb", outPrefix);

      binaryFile = new ovFile(seqStore, binaryName, ovFileFullWrite);
    }

    ovlLen = ovlStore->loadBlockOfOverlaps(ovl, ovlMax);

    while (ovlLen > 0) {
      for (uint32 oo=0; oo<ovlLen; oo++) {
        if (params.filterOverlap(ovl + oo) == true)
          continue;

        if      (asCoords) {
          fputs(ovl[oo].toString(ovlString, ovOverlapAsCoords, true), stdout);
        }

        else if (asHangs) {
          fputs(ovl[oo].toString(ovlString, ovOverlapAsHangs, true), stdout);
        }

        else if (asUnaligned) {
          fputs(ovl[oo].toString(ovlString, ovOverlapAsUnaligned, true), stdout);
        }

        else if (asPAF) {
          fputs(ovl[oo].toString(ovlString, ovOverlapAsPaf, true), stdout);
        }

        else if (asGFA) {
          if ((ovl[oo].overlapAIsContained() == true) ||
              (ovl[oo].overlapBIsContained() == true))
            continue;

          //  If the overlap is off our left end, emit reverse A and (flipped) reverse B.
          if      (ovl[oo].overlapAEndIs5prime()) {
            fprintf(gfaLinks, "L\tread%08u\t-\tread%08u\t%c\t%uM\n",
                    ovl[oo].a_iid, ovl[oo].b_iid, ovl[oo].flipped() ? '+' : '-', ovl[oo].length());
            gfaReads[ovl[oo].a_iid]++;
            gfaReads[ovl[oo].b_iid]++;
          }

          //  If the overlap is off our right end, emit forward A and (flipped?) B.
          else if (ovl[oo].overlapAEndIs3prime()) {
            fprintf(gfaLinks, "L\tread%08u\t+\tread%08u\t%c\t%uM\n",
                    ovl[oo].a_iid, ovl[oo].b_iid, ovl[oo].flipped() ? '-' : '+', ovl[oo].length());
            gfaReads[ovl[oo].a_iid]++;
            gfaReads[ovl[oo].b_iid]++;
          }

          //  And if neither, we shouldn't get here.
          else {
            fputs(ovl[oo].toString(ovlString, ovOverlapAsUnaligned, true), stderr);
            assert(0);
          }

        }

        else if (asBinary) {
          binaryFile->writeOverlap(&ovl[oo]);
        }

        else {
        }
      }

      ovlLen = ovlStore->loadBlockOfOverlaps(ovl, ovlMax);
    }

    //  If writing a GFA output, now that we've output the links we know what
    //  sequences are used and we can write the header block.  Then, the
    //  links are copied to the output.

    if (asGFA) {
      uint32  bufferLen = 0;
      uint32  bufferMax = 1048576;
      char   *buffer = new char [bufferMax];

      //  Close the links file.
      AS_UTL_closeFile(gfaLinks);

      //  Write the header to the actual output file.
      fprintf(gfaFile, "H\tVN:Z:1.0\n");
      for (uint32 ii=1; ii<seqStore->sqStore_lastReadID() + 1; ii++)
        if (gfaReads[ii] > 0)
          fprintf(gfaFile, "S\tread%08u\t*\tLN:i:%u\n", ii, seqStore->sqStore_getReadLength(ii));

      //  Reopen the links file, and copy all data to the output file.
      gfaLinks  = AS_UTL_openInputFile(outPrefix, '.', "gfalinks");
      while (!feof(gfaLinks)) {
        bufferLen = loadFromFile(buffer, "copybuffer", sizeof(char), bufferMax, gfaLinks, false);
        writeToFile(buffer, "copybuffer", sizeof(char), bufferLen, gfaFile);
      }
      AS_UTL_closeFile(gfaFile);

      //  Delete the links.
      AS_UTL_unlink(outPrefix, '.', "gfalinks");

      delete [] gfaReads;
      delete [] buffer;
    }

    if (asBinary) {
      delete binaryFile;
    }
  }

  //
  //  If as a picture, we need to build a list of the overlaps to show,
  //  then draw the picture.
  //

  if (asPicture) {
    for (uint32 rr=bgnID; rr<=endID; rr++) {
      uint32  ovlSav = 0;

      ovlLen = ovlStore->loadOverlapsForRead(rr, ovl, ovlMax);

      for (uint32 oo=0; oo<ovlLen; oo++)
        if (params.filterOverlap(ovl + oo) == false)   //  If not filtered,
          ovl[ovlSav++] = ovl[oo];                     //  save the overlap for drawing

      if (ovlSav > 0)
        params.drawPicture(rr, ovl, ovlSav, seqStore, withScores);
    }
  }

  //
  //  Phew, that was a lot of work.
  //

  delete [] ovl;
  delete    ovlStore;

  delete seqStore;

  exit(0);
}
