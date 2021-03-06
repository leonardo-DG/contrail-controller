/*
 * Copyright (c) 2013 Juniper Networks, Inc. All rights reserved.
 */

#ifndef vnsw_agent_test_init_h
#define vnsw_agent_test_init_h

#include <sys/socket.h>
#include <linux/netlink.h>
#include <net/if.h>
#include <linux/if_tun.h>
#include <linux/if_packet.h>
#include <netinet/ether.h>
#include <pthread.h>

#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>
#include <boost/uuid/string_generator.hpp>
#include <boost/program_options.hpp>
#include <pugixml/pugixml.hpp>

#include <cfg/init_config.h>
#include <oper/operdb_init.h>
#include <controller/controller_init.h>
#include <controller/controller_vrf_export.h>
#include <services/services_init.h>
#include <ksync/ksync_init.h>
#include <ksync/vnswif_listener.h>
#include <cmn/agent_cmn.h>
#include <base/task.h>
#include <base/task_trigger.h>
#include <io/event_manager.h>
#include <base/util.h>
#include <ifmap_agent_parser.h>
#include <ifmap_agent_table.h>
#include <cfg/interface_cfg.h>
#include <cfg/init_config.h>
#include <oper/vn.h>
#include <oper/multicast.h>
#include <oper/vm.h>
#include <oper/interface.h>
#include <oper/inet4_ucroute.h>
#include <oper/inet4_mcroute.h>
#include <oper/vrf_assign.h>
#include <oper/sg.h>
#include <uve/stats_collector.h>
#include <uve/uve_init.h>
#include <uve/uve_client.h>
#include <uve/flow_stats.h>
#include <uve/agent_stats.h>
#include "pkt_gen.h"
#include "pkt/flowtable.h"
#include "testing/gunit.h"
#include "kstate/kstate.h"
#include "pkt/pkt_init.h"
#include "cfg/mirror_cfg.h"
#include "test_kstate.h"
#include "sandesh/sandesh_http.h"
#include "xmpp/test/xmpp_test_util.h"

using namespace std;

#define TUN_INTF_CLONE_DEV "/dev/net/tun"
#define DEFAULT_VNSW_CONFIG_FILE "controller/src/vnsw/agent/test/vnswa_cfg.xml"

#define GETUSERARGS()                           \
    bool ksync_init = false;                    \
    char init_file[1024];                       \
    memset(init_file, '\0', sizeof(init_file)); \
    ::testing::InitGoogleTest(&argc, argv);     \
    namespace opt = boost::program_options;     \
    opt::options_description desc("Options");   \
    opt::variables_map vm;                      \
    desc.add_options()                          \
        ("help", "Print help message")          \
        ("config", opt::value<string>(), "Specify Init config file")  \
        ("kernel", "Run with vrouter");         \
    opt::store(opt::parse_command_line(argc, argv, desc), vm); \
    opt::notify(vm);                            \
    if (vm.count("help")) {                     \
        cout << "Test Help" << endl << desc << endl; \
        exit(0);                                \
    }                                           \
    if (vm.count("kernel")) {                   \
        ksync_init = true;                      \
    }                                           \
    if (vm.count("config")) {                   \
        strncpy(init_file, vm["config"].as<string>().c_str(), (sizeof(init_file) - 1) ); \
    } else {                                    \
        strcpy(init_file, DEFAULT_VNSW_CONFIG_FILE); \
    }                                           \

struct PortInfo {
    char name[32];
    int intf_id;
    char addr[32];
    char mac[32];
    int vn_id;
    int vm_id;
};

struct FlowIp {
    uint32_t sip;
    uint32_t dip;
    char vrf[32];
};

class FlowFlush : public Task {
public:
    FlowFlush() : Task((TaskScheduler::GetInstance()->GetTaskId("FlowFlush")), 0) {
    }
    virtual bool Run() {
        FlowTable::GetFlowTableObject()->DeleteAll();
        return true;
    }
};

class FlowAge : public Task {
public:
    FlowAge() : Task((TaskScheduler::GetInstance()->GetTaskId("FlowAge")), 0) {
    }
    virtual bool Run() {
        //AgentUve::GetInstance()->GetFlowStatsCollector()->SetFlowAgeTime(100);
        //usleep(100);
        AgentUve::GetInstance()->GetFlowStatsCollector()->Run();
        return true;
    }
};

struct IpamInfo {
    char ip_prefix[32];
    int plen;
    char gw[32];
};

class TestClient {
public:
    TestClient() { 
        vn_notify_ = 0;
        vm_notify_ = 0;
        port_notify_ = 0;
        port_del_notify_ = 0;
        vm_del_notify_ = 0;
        vn_del_notify_ = 0;
        vrf_del_notify_ = 0;
        cfg_notify_ = 0;
        acl_notify_ = 0;
        vrf_notify_ = 0;
        mpls_notify_ = 0;
        nh_notify_ = 0;
        mpls_del_notify_ = 0;
        nh_del_notify_ = 0;
        shutdown_done_ = false;
    };
    virtual ~TestClient() { };

    void VrfNotify(DBTablePartBase *partition, DBEntryBase *e) {
        vrf_notify_++;
        if (e->IsDeleted()) {
            vrf_del_notify_++;
        }
    };

    void AclNotify(DBTablePartBase *partition, DBEntryBase *e) {
        acl_notify_++;
    };

    void VnNotify(DBTablePartBase *partition, DBEntryBase *e) {
        vn_notify_++;
        if (e->IsDeleted()) {
            vn_del_notify_++;
        }
    };

    void VmNotify(DBTablePartBase *partition, DBEntryBase *e) {
        vm_notify_++;
        if (e->IsDeleted()) {
            vm_del_notify_++;
        }
    };

    void PortNotify(DBTablePartBase *partition, DBEntryBase *e) {
        const Interface *intf = static_cast<const Interface *>(e);
        port_notify_++;
        if (intf->IsDeleted()) {
            port_del_notify_++;
        }
    };

    void CompositeNHNotify(DBTablePartBase *partition, DBEntryBase *e) {
        const NextHop *nh = static_cast<const NextHop *>(e);
        if (nh->GetType() != NextHop::COMPOSITE) 
            return;
        nh_notify_++;
        if (e->IsDeleted()) {
            nh_del_notify_++;
        }
    };

    void MplsNotify(DBTablePartBase *partition, DBEntryBase *e) {
        mpls_notify_++;
        if (e->IsDeleted()) {
            mpls_del_notify_++;
        }
    };

    void CfgNotify(DBTablePartBase *partition, DBEntryBase *e) {
        cfg_notify_++;
    };

    static void WaitForIdle(int wait_seconds = 1) {
        static const int kTimeout = 1000;
        int i;
        int count = ((wait_seconds * 1000000) / kTimeout);
        TaskScheduler *scheduler = TaskScheduler::GetInstance();
        for (i = 0; i < count; i++) {
            if (scheduler->IsEmpty()) {
                break;
            }
            usleep(kTimeout);
        }
        EXPECT_GT(count, i);
    }

    void VrfReset() {vrf_notify_ = 0;};
    void AclReset() {acl_notify_ = 0;};
    void VnReset() {vn_notify_ = 0;};
    void VmReset() {vm_notify_ = 0;};
    void PortReset() {port_notify_ = port_del_notify_ = 0;};
    void NextHopReset() {nh_notify_ = nh_del_notify_ = 0;};
    void MplsReset() {mpls_notify_ = mpls_del_notify_ = 0;};
    void Reset() {vrf_notify_ = acl_notify_ = port_notify_ = vn_notify_ = 
        vm_notify_ = cfg_notify_ = port_del_notify_ =  
        vm_del_notify_ = vn_del_notify_ = vrf_del_notify_ = 0;};

    void NotifyCfgWait(int cfg_count) {
        int i = 0;

        while (cfg_notify_ != cfg_count) {
            if (i++ < 25) {
                usleep(1000);
            } else {
                break;
            }
        }
    };

    void FlowTimerWait(int count) {
        int i = 0;

        while (AgentUve::GetInstance()->GetFlowStatsCollector()->run_counter_ <= count) {
            if (i++ < 1000) {
                usleep(1000);
            } else {
                break;
            }
        }
        EXPECT_TRUE(AgentUve::GetInstance()->GetFlowStatsCollector()->run_counter_ > count);
        WaitForIdle(2);
    }

    void IfStatsTimerWait(int count) {
        int i = 0;

        while (AgentUve::GetInstance()->GetStatsCollector()->run_counter_ <= count) {
            if (i++ < 1000) {
                usleep(1000);
            } else {
                break;
            }
        }
        EXPECT_TRUE(AgentUve::GetInstance()->GetStatsCollector()->run_counter_ > count);
        WaitForIdle(2);
    }

    void VrfStatsTimerWait(int count) {
        int i = 0;

        while (AgentUve::GetInstance()->GetStatsCollector()->vrf_stats_responses_ <= count) {
            if (i++ < 1000) {
                usleep(1000);
            } else {
                break;
            }
        }
        EXPECT_TRUE(AgentUve::GetInstance()->GetStatsCollector()->vrf_stats_responses_ >= count);
        WaitForIdle(2);
    }

    void DropStatsTimerWait(int count) {
        int i = 0;

        while (AgentUve::GetInstance()->GetStatsCollector()->drop_stats_responses_ <= count) {
            if (i++ < 1000) {
                usleep(1000);
            } else {
                break;
            }
        }
        EXPECT_TRUE(AgentUve::GetInstance()->GetStatsCollector()->drop_stats_responses_ >= count);
        WaitForIdle(2);
    }

    void KStateResponseWait(int count) {
        int i = 0;

        while (TestKStateBase::handler_count_ < count) {
            if (i++ < 100) {
                usleep(1000);
            } else {
                break;
            }
        }
        EXPECT_TRUE(TestKStateBase::handler_count_ >= count);
        WaitForIdle();
    }

    bool AclNotifyWait(int count) {
        int i = 0;

        while (acl_notify_ != count) {
            if (i++ < 25) {
                usleep(1000);
            } else {
                break;
            }
        }

        WaitForIdle();
        EXPECT_EQ(count, acl_notify_);
        return (acl_notify_ == count);
    }

    bool VrfNotifyWait(int count) {
        int i = 0;

        while (vrf_notify_ != count) {
            if (i++ < 25) {
                usleep(1000);
            } else {
                break;
            }
        }

        WaitForIdle();
        EXPECT_EQ(count, vrf_notify_);
        return (vrf_notify_ == count);
    }

    bool VnNotifyWait(int vn_count) {
        int i = 0;

        while (vn_notify_ != vn_count) {
            if (i++ < 25) {
                usleep(1000);
            } else {
                break;
            }
        }

        WaitForIdle();
        EXPECT_EQ(vn_count, vn_notify_);
        return (vn_notify_ == vn_count);
    }

    bool VmNotifyWait(int vm_count) {
        int i = 0;

        while (vm_notify_ != vm_count) {
            if (i++ < 25) {
                usleep(1000);
            } else {
                break;
            }
        }

        WaitForIdle();
        EXPECT_EQ(vm_count, vm_notify_);
        return (vm_notify_ == vm_count);
    }

    bool PortNotifyWait(int port_count) {
        int i = 0;

        while (port_notify_ != port_count) {
            if (i++ < 25) {
                usleep(10000);
            } else {
                break;
            }
        }

        WaitForIdle();
        EXPECT_EQ(port_count, port_notify_);
        return (port_notify_ == port_count);
    }

    bool PortDelNotifyWait(int port_count) {
        int i = 0;

        while (port_del_notify_ != port_count) {
            if (i++ < 100) {
                usleep(10000);
            } else {
                break;
            }
        }

        WaitForIdle();
        EXPECT_EQ(port_count, port_del_notify_);
        return (port_del_notify_ == port_count);
    }
    
    bool VmDelNotifyWait(int count) {
        int i = 0;

        while (vm_del_notify_ != count) {
            if (i++ < 100) {
                usleep(10000);
            } else {
                break;
            }
        }

        WaitForIdle();
        EXPECT_EQ(count, vm_del_notify_);
        return (vm_del_notify_ == count);
    }

    bool VnDelNotifyWait(int count) {
        int i = 0;

        while (vn_del_notify_ != count) {
            if (i++ < 100) {
                usleep(10000);
            } else {
                break;
            }
        }

        WaitForIdle();
        EXPECT_EQ(count, vn_del_notify_);
        return (vn_del_notify_ == count);
    }

    bool VrfDelNotifyWait(int count) {
        int i = 0;

        while (vrf_del_notify_ != count) {
            if (i++ < 100) {
                usleep(10000);
            } else {
                break;
            }
        }

        WaitForIdle();
        EXPECT_EQ(count, vrf_del_notify_);
        return (vrf_del_notify_ == count);
    }

    bool CompositeNHDelWait(int nh_count) {
        int i = 0;

        while (nh_notify_ != nh_count) {
            if (i++ < 100) {
                usleep(10000);
            } else {
                break;
            }
        }

        WaitForIdle();
        EXPECT_EQ(nh_count, nh_del_notify_);
        return (nh_del_notify_ == nh_count);
    }

    bool CompositeNHWait(int nh_count) {
        int i = 0;

        while (nh_notify_ != nh_count) {
            if (i++ < 100) {
                usleep(10000);
            } else {
                break;
            }
        }

        WaitForIdle();
        EXPECT_EQ(nh_count, nh_notify_);
        return (nh_notify_ == nh_count);
    }

    bool MplsDelWait(int mpls_count) {
        int i = 0;

        while (mpls_notify_ != mpls_count) {
            if (i++ < 100) {
                usleep(10000);
            } else {
                break;
            }
        }

        WaitForIdle();
        EXPECT_EQ(mpls_count, mpls_del_notify_);
        return (mpls_del_notify_ == mpls_count);
    }

    bool MplsWait(int mpls_count) {
        int i = 0;

        while (mpls_notify_ != mpls_count) {
            if (i++ < 100) {
                usleep(10000);
            } else {
                break;
            }
        }

        WaitForIdle();
        EXPECT_EQ(mpls_count, mpls_notify_);
        return (mpls_notify_ == mpls_count);
    }
    
    bool NotifyWait(int port_count, int vn_count, int vm_count) {
        bool vn_ret = VnNotifyWait(vn_count);
        bool vm_ret = VmNotifyWait(vm_count);
        bool port_ret = PortNotifyWait(port_count);

        return (vn_ret && vm_ret && port_ret);
    };

    void SetFlowFlushExclusionPolicy() {
        TaskScheduler *scheduler = TaskScheduler::GetInstance();
        TaskPolicy policy;
        policy.push_back(TaskExclusion(scheduler->GetTaskId("Agent::StatsCollector")));
        policy.push_back(TaskExclusion(scheduler->GetTaskId("Agent::FlowHandler")));
        policy.push_back(TaskExclusion(scheduler->GetTaskId("Agent::KSync")));
        scheduler->SetPolicy(scheduler->GetTaskId("FlowFlush"), policy);
    }

    void EnqueueFlowFlush() {
        TaskScheduler *scheduler = TaskScheduler::GetInstance();
        FlowFlush *task = new FlowFlush();
        scheduler->Enqueue(task);
    }

    void SetFlowAgeExclusionPolicy() {
        TaskScheduler *scheduler = TaskScheduler::GetInstance();
        TaskPolicy policy;
        policy.push_back(TaskExclusion(scheduler->GetTaskId("Agent::StatsCollector")));
        scheduler->SetPolicy(scheduler->GetTaskId("FlowAge"), policy);
    }

    void EnqueueFlowAge() {
        TaskScheduler *scheduler = TaskScheduler::GetInstance();
        FlowAge *task = new FlowAge();
        scheduler->Enqueue(task);
    }

    void Init() {
        Agent::GetInstance()->GetIntfCfgTable()->Register(boost::bind(&TestClient::CfgNotify, 
                                                   this, _1, _2));
        Agent::GetInstance()->GetVnTable()->Register(boost::bind(&TestClient::VnNotify, 
                                                   this, _1, _2));
        Agent::GetInstance()->GetVmTable()->Register(boost::bind(&TestClient::VmNotify, 
                                                   this, _1, _2));
        Agent::GetInstance()->GetInterfaceTable()->Register(boost::bind(&TestClient::PortNotify, 
                                                   this, _1, _2));
        Agent::GetInstance()->GetAclTable()->Register(boost::bind(&TestClient::AclNotify, 
                                                   this, _1, _2));
        Agent::GetInstance()->GetVrfTable()->Register(boost::bind(&TestClient::VrfNotify, 
                                                   this, _1, _2));
        Agent::GetInstance()->GetNextHopTable()->Register(boost::bind(&TestClient::CompositeNHNotify,
                                                   this, _1, _2));
        Agent::GetInstance()->GetMplsTable()->Register(boost::bind(&TestClient::MplsNotify, 
                                                   this, _1, _2));
    };

    void Shutdown();

    bool shutdown_done_;
    int vn_notify_;
    int vm_notify_;
    int port_notify_;
    int port_del_notify_;
    int vm_del_notify_;
    int vn_del_notify_;
    int vrf_del_notify_;
    int cfg_notify_;
    int acl_notify_;
    int vrf_notify_;
    int nh_del_notify_;
    int mpls_del_notify_;
    int nh_notify_;
    int mpls_notify_;
};

class AgentTestInit {
public:
    enum State {
        MOD_INIT,
        STATIC_OBJ_OPERDB,
        STATIC_OBJ_PKT,
        CONFIG_INIT,
        CONFIG_RUN,
        INIT_DONE,
        SHUTDOWN,
        SHUTDOWN_DONE,
    };

    AgentTestInit(bool ksync_init, bool pkt_init, bool services_init,
                  const char *init_file, int sandesh_port, bool log,
                  TestClient *client, bool uve_init, 
                  int agent_stats_interval, int flow_stats_interval) 
        : state_(MOD_INIT), ksync_init_(ksync_init), pkt_init_(pkt_init),
        services_init_(services_init), init_file_(init_file),
        sandesh_port_(sandesh_port), log_locally_(log),
        trigger_(NULL), client_(client), uve_init_(uve_init),
        agent_stats_interval_(agent_stats_interval), 
        flow_stats_interval_(flow_stats_interval) {}
    ~AgentTestInit() {
        for (std::vector<TaskTrigger *>::iterator it = list_.begin();
             it != list_.end(); ++it) {
            (*it)->Reset();
            delete *it;
        }
    }

    bool Run();
    void Trigger() {
        trigger_ = new TaskTrigger
            (boost::bind(&AgentTestInit::Run, this), 
             TaskScheduler::GetInstance()->GetTaskId("db::DBTable"), 0);
        list_.push_back(trigger_);
        trigger_->Set();
    }

    void Shutdown() {
        state_ = AgentTestInit::SHUTDOWN;
        Trigger();
        while (state_ != SHUTDOWN_DONE) {
            usleep(1000);
        }
        usleep(1000);
    }

    State GetState() {return state_;};

private:
    void InitModules();
    State state_;
    bool ksync_init_;
    bool pkt_init_;
    bool services_init_;
    const char *init_file_;
    int sandesh_port_;
    bool log_locally_;
    TaskTrigger *trigger_;
    TestClient *client_;
    bool uve_init_;
    int agent_stats_interval_;
    int flow_stats_interval_;
    std::vector<TaskTrigger *> list_;
};

TestClient *TestInit(const char *init_file = NULL, bool ksync_init = false, 
                     bool pkt_init = true, bool services_init = true,
                     bool uve_init = true,
                     int agent_stats_interval = AgentStatsCollector::AgentStatsInterval,
                     int flow_stats_interval = FlowStatsCollector::FlowStatsInterval,
                     bool asio = true);

void TestShutdown();
TestClient *StatsTestInit();

extern TestClient *client;

#endif // vnsw_agent_test_init_h
