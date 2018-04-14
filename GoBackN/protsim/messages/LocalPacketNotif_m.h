//
// Generated file, do not edit! Created by opp_msgc 3.3 from LocalPacketNotif.msg.
//

#ifndef _LOCALPACKETNOTIF_M_H_
#define _LOCALPACKETNOTIF_M_H_

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
 * Class generated from <tt>LocalPacketNotif.msg</tt> by opp_msgc.
 * <pre>
 * message LocalPacketNotif extends NetworkPacket
 * {
 *     fields:
 * 	bool         noRoute;
 * }
 * </pre>
 */
class LocalPacketNotif : public NetworkPacket
{
  protected:
    bool noRoute_var;

    // protected and unimplemented operator==(), to prevent accidental usage
    bool operator==(const LocalPacketNotif&);

  public:
    LocalPacketNotif(const char *name=NULL, int kind=0);
    LocalPacketNotif(const LocalPacketNotif& other);
    virtual ~LocalPacketNotif();
    LocalPacketNotif& operator=(const LocalPacketNotif& other);
    virtual cPolymorphic *dup() const {return new LocalPacketNotif(*this);}
    virtual void netPack(cCommBuffer *b);
    virtual void netUnpack(cCommBuffer *b);

    // field getter/setter methods
    virtual bool getNoRoute() const;
    virtual void setNoRoute(bool noRoute_var);
};

inline void doPacking(cCommBuffer *b, LocalPacketNotif& obj) {obj.netPack(b);}
inline void doUnpacking(cCommBuffer *b, LocalPacketNotif& obj) {obj.netUnpack(b);}

#endif // _LOCALPACKETNOTIF_M_H_