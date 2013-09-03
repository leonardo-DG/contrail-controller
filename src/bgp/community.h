/*
 * Copyright (c) 2013 Juniper Networks, Inc. All rights reserved.
 */

#ifndef ctrlplane_community_h
#define ctrlplane_community_h

#include <set>
#include <vector>
#include <boost/array.hpp>
#include <boost/intrusive_ptr.hpp>
#include <tbb/atomic.h>

#include "bgp/bgp_attr_base.h"
#include "base/parse_object.h"
#include "base/util.h"

class BgpAttr;
class CommunityDB;
class ExtCommunityDB;
class BgpServer;

struct CommunitySpec : public BgpAttribute {
    static const int kSize = -1;
    static const uint8_t kFlags = Optional | Transitive;
    CommunitySpec() : BgpAttribute(Communities, kFlags) { }
    explicit CommunitySpec(const BgpAttribute &rhs) : BgpAttribute(rhs) { }
    std::vector<uint32_t> communities;
    virtual int CompareTo(const BgpAttribute &rhs_attr) const {
        int ret = BgpAttribute::CompareTo(rhs_attr);
        if (ret != 0) return ret;
        KEY_COMPARE(communities,
                    static_cast<const CommunitySpec &>(rhs_attr).communities);
        return 0;
    }
    virtual void ToCanonical(BgpAttr *attr);
    virtual std::string ToString() const;
};

class Community {
public:
    enum WellKnownCommunity {
        NoExport = 0xFFFFFF01,
        NoAdvertise = 0xFFFFFF02,
        NoExportSubconfed = 0xFFFFFF03,
    };

    Community(CommunityDB *comm_db) : comm_db_(comm_db) { refcount_ = 0; }
    explicit Community(CommunityDB *comm_db, const CommunitySpec spec)
        : comm_db_(comm_db), communities_(spec.communities) {
        refcount_ = 0;
    }

    virtual ~Community() { }
    virtual void Remove();
    int CompareTo(const Community &rhs) const;
    const std::vector<uint32_t> &communities() const { return communities_; }

    friend std::size_t hash_value(Community const &comm) {
        size_t hash = 0;
        boost::hash_range(hash, comm.communities_.begin(),
                          comm.communities_.end());
        return hash;
    }

private:
    CommunityDB *comm_db_;

    friend int intrusive_ptr_add_ref(Community *comm);
    friend int intrusive_ptr_del_ref(Community *comm);
    friend void intrusive_ptr_release(Community *comm);
    // atomic refcount
    mutable tbb::atomic<int> refcount_;
    std::vector<uint32_t> communities_;
};

inline int intrusive_ptr_add_ref(Community *comm) {
    return comm->refcount_.fetch_and_increment();
}

inline int intrusive_ptr_del_ref(Community *comm) {
    return comm->refcount_.fetch_and_decrement();
}

inline void intrusive_ptr_release(Community *comm) {
    int prev = comm->refcount_.fetch_and_decrement();
    if (prev == 1) {
        comm->Remove();
        assert(comm->refcount_ == 0);
        delete comm;
    }
}

typedef boost::intrusive_ptr<Community> CommunityPtr;

struct CommunityCompare {
    bool operator()(const Community *lhs, const Community *rhs) {
        return lhs->CompareTo(*rhs) < 0;
    }
};

class CommunityDB : public BgpPathAttributeDB<Community, CommunityPtr,
                                              CommunitySpec, CommunityCompare,
                                              CommunityDB> {
public:
    CommunityDB(BgpServer *server);
    virtual ~CommunityDB() { }

private:
    BgpServer *server_;
};

struct ExtCommunitySpec : public BgpAttribute {
    static const int kSize = -1;
    static const uint8_t kFlags = Optional | Transitive;
    ExtCommunitySpec() : BgpAttribute(ExtendedCommunities, kFlags) { }
    explicit ExtCommunitySpec(const BgpAttribute &rhs) : BgpAttribute(rhs) { }
    std::vector<uint64_t> communities;
    virtual int CompareTo(const BgpAttribute &rhs_attr) const {
        int ret = BgpAttribute::CompareTo(rhs_attr);
        if (ret != 0) return ret;
        KEY_COMPARE(communities,
                    static_cast<const ExtCommunitySpec &>(rhs_attr).communities);
        return 0;
    }
    virtual void ToCanonical(BgpAttr *attr);
    virtual std::string ToString() const;
};

class ExtCommunity {
public:
    typedef boost::array<uint8_t, 8> ExtCommunityValue;
    typedef std::vector<ExtCommunityValue> ExtCommunityList;
    ExtCommunity(ExtCommunityDB *extcomm_db) : extcomm_db_(extcomm_db) {
        refcount_ = 0;
    }

    // Copy constructor
    explicit ExtCommunity(const ExtCommunity &rhs)
        : extcomm_db_(rhs.extcomm_db_), communities_(rhs.communities_) {
        refcount_ = 0;
    }

    explicit ExtCommunity(ExtCommunityDB *extcomm_db,
                          const ExtCommunitySpec spec);
    virtual ~ExtCommunity() { }
    virtual void Remove();
    int CompareTo(const ExtCommunity &rhs) const;
    // Add new community.. Added for appending export_rt
    void Append(const ExtCommunityList &export_rt);
    void RemoveRTarget();
    void RemoveSGID();

    // Return vector of communities
    const ExtCommunityList &communities() const {
        return communities_;
    }

    static bool is_route_target(const ExtCommunityValue &val) {
        //
        // Route target extended community
        // 1. 2 Octet AS specific extended community Route Target
        // 2. IPv4 Address specific extended community Route Target
        // 3. 4 Octet AS specific extended community Route Target
        //
        return ((val[0] == 0x0 || (val[0] == 0x2) || (val[0] ==  0x1)) && 
                (val[1] == 0x2));
    }

    static bool is_security_group(const ExtCommunityValue &val) {
        //
        // SG ID extended community
        // 2 Octet AS specific extended community
        //
        return (val[0] == 0x80) && (val[1] == 0x4);
    }

    static bool is_tunnel_encap(const ExtCommunityValue &val) {
        // Tunnel encap extended community
        return (val[0] == 0x03) && (val[1] == 0x0C);
    }

    friend std::size_t hash_value(ExtCommunity const &comm) {
        size_t hash = 0;
        for (ExtCommunityList::const_iterator iter = comm.communities_.begin();
                iter != comm.communities_.end(); iter++) {
            boost::hash_range(hash, iter->begin(), iter->end());
        }
        return hash;
    }

private:
    ExtCommunityDB *extcomm_db_;

    friend int intrusive_ptr_add_ref(const ExtCommunity *comm);
    friend int intrusive_ptr_del_ref(const ExtCommunity *comm);
    friend void intrusive_ptr_release(ExtCommunity *comm);
    // atomic refcount
    mutable tbb::atomic<int> refcount_;
    ExtCommunityList communities_;
};

inline int intrusive_ptr_add_ref(const ExtCommunity *comm) {
    return comm->refcount_.fetch_and_increment();
}

inline int intrusive_ptr_del_ref(const ExtCommunity *comm) {
    return comm->refcount_.fetch_and_decrement();
}

inline void intrusive_ptr_release(ExtCommunity *comm) {
    int prev = comm->refcount_.fetch_and_decrement();
    if (prev == 1) {
        comm->Remove();
        assert(comm->refcount_ == 0);
        delete comm;
    }
}

typedef boost::intrusive_ptr<ExtCommunity> ExtCommunityPtr;

struct ExtCommunityCompare {
    bool operator()(const ExtCommunity *lhs, const ExtCommunity *rhs) {
        return lhs->CompareTo(*rhs) < 0;
    }
};

class ExtCommunityDB : public BgpPathAttributeDB<ExtCommunity, ExtCommunityPtr,
                                                 ExtCommunitySpec,
                                                 ExtCommunityCompare,
                                                 ExtCommunityDB> {
public:
    ExtCommunityDB(BgpServer *server);

    // Added for copying and appending RT as per export rules
    ExtCommunityPtr AppendAndLocate(const ExtCommunity *me, 
                            const ExtCommunity::ExtCommunityList &export_rt);

    ExtCommunityPtr ReplaceRTargetAndLocate(const ExtCommunity *src,
                            const ExtCommunity::ExtCommunityList &export_list);
    ExtCommunityPtr ReplaceSGIDListAndLocate(const ExtCommunity *src,
                            const ExtCommunity::ExtCommunityList &sgid_list);
private:
    BgpServer *server_;
};

#endif