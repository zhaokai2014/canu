
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
 *  Modifications by:
 *
 *    Brian P. Walenz from 2014-DEC-22 to 2015-AUG-14
 *      are Copyright 2014-2015 Battelle National Biodefense Institute, and
 *      are subject to the BSD 3-Clause License
 *
 *    Brian P. Walenz beginning on 2015-OCT-30
 *      are a 'United States Government Work', and
 *      are released in the public domain
 *
 *  File 'README.licenses' in the root directory of this distribution contains
 *  full conditions and disclaimers for each license.
 */

#ifndef TG_TIG_H
#define TG_TIG_H

#include "AS_global.H"

#include "sqStore.H"
#include "bits.H"

#include <map>
using namespace std;


typedef enum {
  tgTig_noclass      = 0x00,    //  0x02 was for bubbles, but that was moved
  tgTig_unassembled  = 0x01,    //  to be only a suggestion.
  tgTig_contig       = 0x03     //  tgTig_contig MUST be 3 for compatibility.
} tgTig_class;

static
const
char *
toString(tgTig_class c) {
  switch (c) {
    case tgTig_noclass:      return("unsetc");  break;
    case tgTig_unassembled:  return("unassm");  break;
    case tgTig_contig:       return("contig");  break;
  }

  return("undefined-class");
}

static
tgTig_class
toClass(char const *c) {
  if (strcmp(c, "unsetc") == 0)
    return(tgTig_noclass);
  if (strcmp(c, "unassm") == 0)
    return(tgTig_unassembled);
  if (strcmp(c, "contig") == 0)
    return(tgTig_contig);

  fprintf(stderr, "WARNING:  tiTig_class toClass('%s') is not a valid class.\n", c);

  return(tgTig_noclass);
}




//  Info about the placement of an object in a tig.  For unitigs, this
//  will be just reads.  For contigs, this could be unitigs and reads.
//
class tgPosition {
public:
  tgPosition();

  //  Accessors

  uint32               ident(void)     const { return(_objID); };

  bool                 isRead(void)    const { return(_isRead   == true); };
  bool                 isUnitig(void)  const { return(_isUnitig == true); };
  bool                 isContig(void)  const { return(_isContig == true); };

  uint32               anchor(void)          { return(_anchor); };
  int32                aHang(void)           { return(_ahang);  };
  int32                bHang(void)           { return(_bhang);  };

  bool                 isForward(void) const { return(_isReverse == false); };
  bool                 isReverse(void) const { return(_isReverse == true);  };

  //  Position in the parent, both oriented (bgn/end) and unoriented (min/max).

  int32                bgn(void) const       { return((_isReverse == false) ? _min : _max); };
  int32                end(void) const       { return((_isReverse == false) ? _max : _min); };

  int32                min(void) const       { return(_min); };
  int32                max(void) const       { return(_max); };

  //  Amount of this object to ignore; e.g., trim from the ends.

  int32                askip(void) const       { return(_askip); };
  int32                bskip(void) const       { return(_bskip); };

  //  Delta values for the alignment to the parent.

  uint32               deltaOffset(void)     { return(_deltaOffset); };
  uint32               deltaLength(void)     { return(_deltaLen);    };

  //  Set just the anchor and hangs, leave positions alone.
  void                 setAnchor(uint32 anchor, int32 ahang, int32 bhang) {
    _anchor = anchor;
    _ahang  = ahang;
    _bhang  = bhang;
  };

  //  Set everything.  This is to be used by unitigger.
  void                 set(uint32 id, uint32 anchor, int32 ahang, int32 bhang, int32  bgn, int32  end) {

    _objID     = id;

    _isRead    = true;
    _isUnitig  = false;
    _isContig  = false;

    _anchor    = anchor;
    _ahang     = ahang;
    _bhang     = bhang;

    _askip     = 0;
    _bskip     = 0;

    if (bgn < end) {
      _min       = bgn;
      _max       = end;
      _isReverse = false;
    } else {
      _min       = end;
      _max       = bgn;
      _isReverse = true;
    }

    _deltaOffset = 0;
    _deltaLen    = 0;
  };

  //  Set the coords, ignoring orientation.
  void                 setMinMax(int32 min, int32 max) {
    _min = min;
    _max = max;
  };

  //  Operators

  bool                 operator<(tgPosition const &that) const {
    int32 al = min();
    int32 bl = that.min();

    if (al != bl)
      return(al < bl);

    if (that._anchor == _objID)  //  I'm his anchor, I must be before it.
      return(true);

    if (_anchor == that._objID)  //  My anchor is the other tig; I must
      return(false);             //  be after it.

    int32 ah = max();
    int32 bh = that.max();

    return(ah > bh);
  }

private:
public:
  uint32              _objID;            //  ID of this object

  uint32              _isRead      : 1;  //  A full length read alignment
  uint32              _isUnitig    : 1;  //
  uint32              _isContig    : 1;  //

  uint32              _isReverse   : 1;  //  Child is oriented forward relative to parent, used during consensus.

  uint32              _spare       : 28;

  uint32              _anchor;           //  ID of the like-type object we align to
  int32               _ahang;            //  Placement relative to anchor object
  int32               _bhang;            //

  int32               _askip;            //  Amount of sequence to not align on each end
  int32               _bskip;            //

  //  Must be signed, utgcns can push reads negative.
  int32               _min;
  int32               _max;

  uint32              _deltaOffset;      //  Our delta alignment to the parent object consensus
  uint32              _deltaLen;
};


class tgTig;  //  Early declaration, for use in tgTigRecord operator=


//  On-disk tig, same as tgTig without the pointers
class tgTigRecord {
public:
  tgTigRecord();
  tgTigRecord(tgTig &tg) { *this = tg; };   //  to support tgTigRecord tr = tgtig

  tgTigRecord         &operator=(tgTig & tg);

private:
public:
  uint32              _tigID;

  uint32              _sourceID;
  uint32              _sourceBgn;
  uint32              _sourceEnd;

  tgTig_class         _class           : 2;
  uint32              _suggestRepeat   : 1;
  uint32              _suggestCircular : 1;
  uint32              _suggestBubble   : 1;
  uint32              _spare           : 27;

  uint32              _layoutLen;
  uint32              _basesLen;
  uint32              _childrenLen;
  uint32              _childDeltaBitsLen;
};


//  Former MultiAlignT
//  In core data
class tgTig {
public:
  tgTig();    //  All data unallocated, lengths set to zero
  ~tgTig();   //  Releases memory

  //  Accessors

  uint32               tigID(void)                 { return(_tigID); };

  bool                 consensusExists(void)       { return(_basesLen > 0); };
  double               computeCoverage(void);

  //  Should be private, but used in tgStoreLoad.C for testing cns files.
public:
  uint32               length(void)                { return(_layoutLen); };
  char                *bases(void)                 { return(_bases);     };
  uint8               *quals(void)                 { return(_quals);     };

public:
  uint32               numberOfChildren(void)      {                            return(_childrenLen); };
  tgPosition          *getChild(uint32 c)          { assert(c < _childrenLen);  return(_children + c);  };
  tgPosition          *addChild(void)              {                            return(_children + _childrenLen++); };

  //  Operators

  void                 clear(void);    //  Clears data but doesn't release memory.

  tgTig               &operator=(tgTigRecord & tr);
  tgTig               &operator=(tgTig &tg);

  bool                 loadFromStreamOrLayout(FILE *F);

  void                 saveToBuffer(writeBuffer *B);
  bool                 loadFromBuffer(readBuffer *B);

  void                 saveToStream(FILE *F);
  bool                 loadFromStream(FILE *F);

  void                 dumpLayout(FILE *F);
  bool                 loadLayout(FILE *F);

  //  Save and load a package of data needed to process this tig.

  void                 exportData(writeBuffer                *exportDataFile,
                                  sqStore                    *seqStore,
                                  bool                        isForCorrection);

  bool                 importData(readBuffer                 *importDataFile,
                                  map<uint32, sqRead *>      &reads,
                                  FILE                       *layoutOutput,
                                  FILE                       *sequenceOutput);


  void                 reverseComplement(void);  //  Does NOT update childDeltas

  void                 dumpFASTA(FILE *F);
  void                 dumpFASTQ(FILE *F);

  //  There are two multiAlign displays; this one, and one in abMultiAlign.
  void                 display(FILE     *F,
                               sqStore  *seq,
                               uint32    displayWidth    = 100,    //  Width of display
                               uint32    displaySpacing  = 3,      //  Space between reads on the same line
                               bool      withQV          = false,
                               bool      withDots        = false);



private:
public:
  uint32              _tigID;             //  ID in the store, or UINT32_MAX if not set

  uint32              _sourceID;          //  Object we are placed in.
  uint32              _sourceBgn;         //  Position of object in source - for a corrected read
  uint32              _sourceEnd;         //  this is the (approximate) position in the raw read.

  //  A variety of flags to suggest what type of unitig this is

  tgTig_class         _class           : 2;    //  Output classification: unassembled, bubble, contig
  uint32              _suggestRepeat   : 1;    //  Bogart made this from detected repeat.
  uint32              _suggestCircular : 1;    //  Bogart found overlaps making a circle.
  uint32              _suggestBubble   : 1;    //  Bogart thinks this is a bubble.

  uint32              _spare           : 32 - 2 - 3;

  uint32              _layoutLen;         //  The max coord in the layout.  Same as _basesLen if _bases exists.

  uint32              _basesLen;          //  Doesn't include the NUL.
  uint32              _basesMax;          //  Does    include the NUL.
  char               *_bases;             //  Linear consensus sequence.
  uint8              *_quals;             //

  tgPosition         *_children;          //  positions of objects that make up this tig
  uint32              _childrenLen;
  uint32              _childrenMax;

  uint32              _childDeltaBitsLen;
  stuffedBits        *_childDeltaBits;

  //  Flags for computing consensus/multialignments

  uint32              _utgcns_verboseLevel;
};


#endif
