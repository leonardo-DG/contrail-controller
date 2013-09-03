/*
 * Copyright (c) 2013 Juniper Networks, Inc. All rights reserved.
 */

#include <boost/foreach.hpp>
#include <pthread.h>
#include "bgp/bgp_attr.h"

#include "base/logging.h"
#include "base/task.h"
#include "base/test/task_test_util.h"
#include "bgp/bgp_log.h"
#include "bgp/bgp_server.h"
#include "control-node/control_node.h"
#include "io/event_manager.h"
#include "testing/gunit.h"

class BgpAttrTest : public ::testing::Test {
protected:
    BgpAttrTest()
        : server_(&evm_),
          attr_db_(server_.attr_db()),
          aspath_db_(server_.aspath_db()),
          comm_db_(server_.comm_db()),
          extcomm_db_(server_.extcomm_db()),
          origin_vn_db_(server_.origin_vn_db()) {
    }

    void TearDown() {
        server_.Shutdown();
        task_util::WaitForIdle();
    }

    EventManager evm_;
    BgpServer server_;
    BgpAttrDB *attr_db_;
    AsPathDB *aspath_db_;
    CommunityDB *comm_db_;
    ExtCommunityDB *extcomm_db_;
    OriginVnDB *origin_vn_db_;
};

TEST_F(BgpAttrTest, UnknownCode) {
    BgpAttrSpec spec;

    BgpAttrOrigin origin(BgpAttrOrigin::IGP);
    spec.push_back(&origin);
    BgpAttribute unknown(BgpAttribute::Reserved, BgpAttribute::Optional);
    spec.push_back(&unknown);
    BgpAttrLocalPref local_pref(1729);
    spec.push_back(&local_pref);

    BgpAttrPtr attr = attr_db_->Locate(spec);
    EXPECT_EQ(BgpAttrOrigin::IGP, attr->origin());
    EXPECT_EQ(1729, attr->local_pref());
}

TEST_F(BgpAttrTest, AsPathAdd) {
    AsPathSpec path;

    // Add to an empty path
    AsPathSpec *p2 = path.Add(1000);
    EXPECT_EQ(1, p2->path_segments.size());
    EXPECT_EQ(1, p2->path_segments[0]->path_segment.size());
    EXPECT_EQ(1000, p2->path_segments[0]->path_segment[0]);
    delete p2;

    // populate the path with some entries
    for (int i = 0; i < 10; i++) {
        AsPathSpec::PathSegment *ps = new AsPathSpec::PathSegment;
        ps->path_segment_type = AsPathSpec::PathSegment::AS_SEQUENCE;
        for (int j = 0; j < 10; j++) {
            ps->path_segment.push_back((i<<4) + j);
        }
        path.path_segments.push_back(ps);
    }

    p2 = path.Add(1000);
    EXPECT_EQ(10, p2->path_segments.size());
    EXPECT_EQ(11, p2->path_segments[0]->path_segment.size());
    EXPECT_EQ(1000, p2->path_segments[0]->path_segment[0]);
    delete p2;

    path.path_segments[0]->path_segment_type = AsPathSpec::PathSegment::AS_SET;
    p2 = path.Add(1000);
    EXPECT_EQ(11, p2->path_segments.size());
    EXPECT_EQ(1, p2->path_segments[0]->path_segment.size());
    EXPECT_EQ(1000, p2->path_segments[0]->path_segment[0]);
    delete p2;

    path.path_segments[0]->path_segment_type = AsPathSpec::PathSegment::AS_SEQUENCE;
    for (int j = 11; j < 256; j++) {
        path.path_segments[0]->path_segment.push_back(j);
    }

    p2 = path.Add(1000);
    EXPECT_EQ(11, p2->path_segments.size());
    EXPECT_EQ(1, p2->path_segments[0]->path_segment.size());
    EXPECT_EQ(1000, p2->path_segments[0]->path_segment[0]);
    delete p2;

}

TEST_F(BgpAttrTest, OriginVnBasic1) {
    OriginVnSpec ovn_spec("red");
    OriginVnPtr ovn = origin_vn_db_->Locate(ovn_spec);
    EXPECT_EQ(1, origin_vn_db_->Size());
    ovn.reset();
    EXPECT_EQ(0, origin_vn_db_->Size());
}

TEST_F(BgpAttrTest, OriginVnBasic2) {
    OriginVnSpec ovn_spec1("red");
    OriginVnSpec ovn_spec2("red");

    OriginVnPtr ovn1 = origin_vn_db_->Locate(ovn_spec1);
    OriginVnPtr ovn2 = origin_vn_db_->Locate(ovn_spec2);
    EXPECT_EQ(ovn1, ovn2);
    EXPECT_EQ(1, origin_vn_db_->Size());
}

TEST_F(BgpAttrTest, OriginVnBasic3) {
    OriginVnSpec ovn_spec1("red1");
    OriginVnSpec ovn_spec2("red2");

    OriginVnPtr ovn1 = origin_vn_db_->Locate(ovn_spec1);
    OriginVnPtr ovn2 = origin_vn_db_->Locate(ovn_spec2);
    EXPECT_NE(ovn1, ovn2);
    EXPECT_EQ(2, origin_vn_db_->Size());
}

TEST_F(BgpAttrTest, OriginVnBasic4) {
    BgpAttrSpec attr_spec;
    OriginVnSpec ovn_spec("red");
    attr_spec.push_back(&ovn_spec);
    BgpAttrPtr ptr = attr_db_->Locate(attr_spec);
    EXPECT_EQ(1, attr_db_->Size());
    EXPECT_EQ("red", ptr->origin_vn()->origin_vn().origin_vn);
    BgpAttr attr(*(ptr.get()));
    EXPECT_EQ("red", attr.origin_vn()->origin_vn().origin_vn);
}

TEST_F(BgpAttrTest, SourceRdBasic1) {
    BgpAttrSpec attr_spec;
    RouteDistinguisher rd = RouteDistinguisher::FromString("192.168.0.1:1");
    BgpAttrSourceRd rd_spec(rd);
    attr_spec.push_back(&rd_spec);
    BgpAttrPtr ptr = attr_db_->Locate(attr_spec);
    EXPECT_EQ(1, attr_db_->Size());
    EXPECT_FALSE(ptr->source_rd().IsNull());
    EXPECT_EQ(rd, ptr->source_rd());
    BgpAttr attr(*(ptr.get()));
    EXPECT_EQ(attr.source_rd(), ptr->source_rd());
}

TEST_F(BgpAttrTest, SourceRdBasic2) {
    BgpAttrSpec attr_spec1;
    BgpAttrLocalPref lpref_spec1(100);
    attr_spec1.push_back(&lpref_spec1);
    BgpAttrSourceRd rd_spec1(RouteDistinguisher::FromString("192.168.0.1:1"));
    attr_spec1.push_back(&rd_spec1);
    BgpAttrPtr ptr1 = attr_db_->Locate(attr_spec1);

    BgpAttrSpec attr_spec2;
    BgpAttrLocalPref lpref_spec2(100);
    attr_spec2.push_back(&lpref_spec2);
    BgpAttrSourceRd rd_spec2(RouteDistinguisher::FromString("192.168.0.1:2"));
    attr_spec2.push_back(&rd_spec2);
    BgpAttrPtr ptr2 = attr_db_->Locate(attr_spec2);

    EXPECT_NE(ptr1, ptr2);
    EXPECT_EQ(2, attr_db_->Size());
}

TEST_F(BgpAttrTest, SourceRdBasic3) {
    BgpAttrSpec attr_spec;

    RouteDistinguisher rd1 = RouteDistinguisher::FromString("192.168.0.1:1");
    BgpAttrSourceRd rd_spec(rd1);
    attr_spec.push_back(&rd_spec);
    BgpAttrPtr ptr = attr_db_->Locate(attr_spec);
    EXPECT_EQ(1, attr_db_->Size());
    EXPECT_EQ(rd1, ptr->source_rd());

    RouteDistinguisher rd2 = RouteDistinguisher::FromString("192.168.0.1:2");
    ptr = attr_db_->ReplaceSourceRdAndLocate(ptr.get(), rd2);
    EXPECT_EQ(1, attr_db_->Size());
    EXPECT_EQ(rd2, ptr->source_rd());
}

TEST_F(BgpAttrTest, BgpAttrDB) {
    BgpAttrSpec spec;
    BgpAttrOrigin *origin = new BgpAttrOrigin(BgpAttrOrigin::INCOMPLETE);
    spec.push_back(origin);

    BgpAttrNextHop *nexthop = new BgpAttrNextHop(0xabcdef01);
    spec.push_back(nexthop);

    BgpAttrAtomicAggregate *aa = new BgpAttrAtomicAggregate;
    spec.push_back(aa);

    BgpAttrAggregator *agg = new BgpAttrAggregator(0xface, 0xcafebabe);
    spec.push_back(agg);

    AsPathSpec *path_spec = new AsPathSpec;
    AsPathSpec::PathSegment *ps = new AsPathSpec::PathSegment;
    ps->path_segment_type = AsPathSpec::PathSegment::AS_SET;
    ps->path_segment.push_back(20);
    ps->path_segment.push_back(21);
    ps->path_segment.push_back(22);
    path_spec->path_segments.push_back(ps);
    spec.push_back(path_spec);

    CommunitySpec *community = new CommunitySpec;
    community->communities.push_back(0x87654321);
    spec.push_back(community);

    ExtCommunitySpec *ext_community = new ExtCommunitySpec;
    ext_community->communities.push_back(0x1020304050607080);
    spec.push_back(ext_community);

    OriginVnSpec *ovn_spec = new OriginVnSpec("red");
    spec.push_back(ovn_spec);

    BgpAttrPtr ptr1 = attr_db_->Locate(spec);
    BgpAttrPtr ptr2 = attr_db_->Locate(spec);

    EXPECT_EQ(1, attr_db_->Size());
    EXPECT_EQ(1, aspath_db_->Size());
    EXPECT_EQ(1, comm_db_->Size());
    EXPECT_EQ(1, extcomm_db_->Size());
    EXPECT_EQ(1, origin_vn_db_->Size());

    agg->address = 0xcafed00d;
    BgpAttrPtr ptr3 = attr_db_->Locate(spec);

    EXPECT_EQ(2, attr_db_->Size());
    EXPECT_EQ(1, aspath_db_->Size());
    EXPECT_EQ(1, comm_db_->Size());
    EXPECT_EQ(1, extcomm_db_->Size());
    EXPECT_EQ(1, origin_vn_db_->Size());

    ptr1.reset();
    EXPECT_EQ(2, attr_db_->Size());
    EXPECT_EQ(1, aspath_db_->Size());
    EXPECT_EQ(1, comm_db_->Size());
    EXPECT_EQ(1, extcomm_db_->Size());
    EXPECT_EQ(1, origin_vn_db_->Size());

    ptr2.reset();
    EXPECT_EQ(1, attr_db_->Size());
    EXPECT_EQ(1, aspath_db_->Size());
    EXPECT_EQ(1, comm_db_->Size());
    EXPECT_EQ(1, extcomm_db_->Size());
    EXPECT_EQ(1, origin_vn_db_->Size());

    ptr3.reset();
    EXPECT_EQ(0, attr_db_->Size());
    EXPECT_EQ(0, aspath_db_->Size());
    EXPECT_EQ(0, comm_db_->Size());
    EXPECT_EQ(0, extcomm_db_->Size());
    EXPECT_EQ(0, origin_vn_db_->Size());

    ptr1 = attr_db_->Locate(spec);
    EXPECT_EQ(1, attr_db_->Size());
    EXPECT_EQ(1, aspath_db_->Size());
    EXPECT_EQ(1, comm_db_->Size());
    EXPECT_EQ(1, extcomm_db_->Size());
    EXPECT_EQ(1, origin_vn_db_->Size());

    STLDeleteValues(&spec);
}

// ----- Test multi-threaded issues in path attributes db.
// Launch a number of threads, that add and delete the same attribute content.
// Since many threads are launched, we get to uncover most of the concurrency
// issues during data base update, if any.

template <class Type, class TypeDB, class TypeSpec>
class AttributeMock : public Type {
public:
    AttributeMock(TypeDB *db, const TypeSpec &spec) : Type(db, spec) { }

    virtual void Remove() {

        // Inject artificial delay to detect concurrency issues, if any.
        usleep(10000);
        Type::Remove();
    }
};

template <class Type, class TypePtr, class TypeDB, class TypeSpec>
static void *ConcurrencyThreadRun(void *objp) {
    TypeDB *db = reinterpret_cast<TypeDB *>(objp);

    TypePtr ptr = db->Locate(new AttributeMock<Type, TypeDB, TypeSpec>(
                                  db, TypeSpec()));
    EXPECT_EQ(1, db->Size());
    return NULL;
}

template <class Type, class TypePtr, class TypeDB, class TypeSpec>
static void ConcurrencyTest(TypeDB *db) {
    std::vector<pthread_t> thread_ids;
    pthread_t tid;

    int thread_count = 1024;
    char *str = getenv("THREAD_COUNT");
    if (str) thread_count = strtoul(str, NULL, 0);

    for (int i = 0; i < thread_count; i++) {
        if (!pthread_create(&tid, NULL,
                            &ConcurrencyThreadRun<Type, TypePtr, TypeDB,
                                                  TypeSpec>,
                            db)) {
            thread_ids.push_back(tid);
        }
    }

    BOOST_FOREACH(tid, thread_ids) { pthread_join(tid, NULL); }
    TASK_UTIL_EXPECT_EQ(0, db->Size());
}

// Instantiate the template functions.
template void ConcurrencyTest<BgpAttr, BgpAttrPtr, BgpAttrDB, BgpAttrSpec>(
                  BgpAttrDB *);
template void ConcurrencyTest<AsPath, AsPathPtr, AsPathDB, AsPathSpec>(
                  AsPathDB *);
template void ConcurrencyTest<Community, CommunityPtr, CommunityDB,
                              CommunitySpec>(CommunityDB *);
template void ConcurrencyTest<ExtCommunity, ExtCommunityPtr, ExtCommunityDB,
                              ExtCommunitySpec>(ExtCommunityDB *);
template void ConcurrencyTest<OriginVn, OriginVnPtr, OriginVnDB, OriginVnSpec>(
                  OriginVnDB *);

TEST_F(BgpAttrTest, BgpAttrDBConcurrency) {
    ConcurrencyTest<BgpAttr, BgpAttrPtr, BgpAttrDB, BgpAttrSpec>(attr_db_);
}

TEST_F(BgpAttrTest, AsPathDBConcurrency) {
    ConcurrencyTest<AsPath, AsPathPtr, AsPathDB, AsPathSpec>(aspath_db_);
}

TEST_F(BgpAttrTest, CommunityDBConcurrency) {
    ConcurrencyTest<Community, CommunityPtr, CommunityDB,
                    CommunitySpec>(comm_db_);
}

TEST_F(BgpAttrTest, ExtCommunityDBConcurrency) {
    ConcurrencyTest<ExtCommunity, ExtCommunityPtr, ExtCommunityDB,
                    ExtCommunitySpec>(extcomm_db_);
}

TEST_F(BgpAttrTest, OriginVnDBConcurrency) {
    ConcurrencyTest<OriginVn, OriginVnPtr, OriginVnDB,
                    OriginVnSpec>(origin_vn_db_);
}

static void SetUp() {
    bgp_log_test::init();
    ControlNode::SetDefaultSchedulingPolicy();
}

static void TearDown() {
    task_util::WaitForIdle();
    TaskScheduler *scheduler = TaskScheduler::GetInstance();
    scheduler->Terminate();
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    SetUp();
    int result = RUN_ALL_TESTS();
    TearDown();
    return result;
}