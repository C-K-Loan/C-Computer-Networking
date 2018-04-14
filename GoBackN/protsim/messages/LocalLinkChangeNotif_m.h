//
// Generated file, do not edit! Created by opp_msgc 3.3 from LocalLinkChangeNotif.msg.
//

#ifndef _LOCALLINKCHANGENOTIF_M_H_
#define _LOCALLINKCHANGENOTIF_M_H_

#include <omnetpp.h>

// opp_msgc version check
#define MSGC_VERSION 0x0303
#if (MSGC_VERSION!=OMNETPP_VERSION)
#    error Version mismatch! Probably this file was generated by an earlier version of opp_msgc: 'make clean' should help.
#endif

// cplusplus {{ ... }} section:

#include "NetworkPacket_m.h"
// end cplusplus


/**
 * Class generated from <tt>LocalLinkChangeNotif.msg</tt> by opp_msgc.
 * <pre>
 * message LocalLinkChangeNotif extends NetworkPacket
 * {
 *     fields:
 * 	unsigned int linkIndex;
 * 	long         nextHop;
 * 	double       newCost;
 * 	double       oldCost;
 * }
 * </pre>
 */
class LocalLinkChangeNotif : public NetworkPacket
{
  protected:
    unsigned int linkIndex_var;
    long nextHop_var;
    double newCost_var;
    double oldCost_var;

    // protected and unimplemented operator==(), to prevent accidental usage
    bool operator==(const LocalLinkChangeNotif&);

  public:
    LocalLinkChangeNotif(const char *name=NULL, int kind=0);
    LocalLinkChangeNotif(const LocalLinkChangeNotif& other);
    virtual ~LocalLinkChangeNotif();
    LocalLinkChangeNotif& operator=(const LocalLinkChangeNotif& other);
    virtual cPolymorphic *dup() const {return new LocalLinkChangeNotif(*this);}
    virtual void netPack(cCommBuffer *b);
    virtual void netUnpack(cCommBuffer *b);

    // field getter/setter methods
    virtual unsigned int getLinkIndex() const;
    virtual void setLinkIndex(unsigned int linkIndex_var);
    virtual long getNextHop() const;
    virtual void setNextHop(long nextHop_var);
    virtual double getNewCost() const;
    virtual void setNewCost(double newCost_var);
    virtual double getOldCost() const;
    virtual void setOldCost(double oldCost_var);
};

inline void doPacking(cCommBuffer *b, LocalLinkChangeNotif& obj) {obj.netPack(b);}
inline void doUnpacking(cCommBuffer *b, LocalLinkChangeNotif& obj) {obj.netUnpack(b);}

#endif // _LOCALLINKCHANGENOTIF_M_H_
