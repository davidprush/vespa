// Copyright 2017 Yahoo Holdings. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
#include "distributorconfiguration.h"
#include <vespa/document/select/parser.h>
#include <vespa/document/select/traversingvisitor.h>
#include <vespa/vespalib/util/exceptions.h>
#include <sstream>

#include <vespa/log/log.h>
LOG_SETUP(".distributorconfiguration");

using namespace std::chrono;

namespace storage {

DistributorConfiguration::DistributorConfiguration(StorageComponent& component)
    : _component(component),
      _byteCountSplitLimit(0xffffffff),
      _docCountSplitLimit(0xffffffff),
      _byteCountJoinLimit(0),
      _docCountJoinLimit(0),
      _minimalBucketSplit(16),
      _maxIdealStateOperations(100),
      _idealStateChunkSize(1000),
      _maxNodesPerMerge(16),
      _lastGarbageCollectionChange(0),
      _garbageCollectionInterval(0),
      _minPendingMaintenanceOps(100),
      _maxPendingMaintenanceOps(1000),
      _maxVisitorsPerNodePerClientVisitor(4),
      _minBucketsPerVisitor(5),
      _maxClusterClockSkew(0),
      _inhibitMergeSendingOnBusyNodeDuration(std::chrono::seconds(60)),
      _doInlineSplit(true),
      _enableJoinForSiblingLessBuckets(false),
      _enableInconsistentJoin(false),
      _enableHostInfoReporting(true),
      _disableBucketActivation(false),
      _sequenceMutatingOperations(true),
      _allowStaleReadsDuringClusterStateTransitions(false),
      _minimumReplicaCountingMode(ReplicaCountingMode::TRUSTED)
{ }

DistributorConfiguration::~DistributorConfiguration() = default;

namespace {

class TimeVisitor : public document::select::TraversingVisitor {
public:
    bool hasCurrentTime;

    TimeVisitor() : hasCurrentTime(false) {}

    void visitCurrentTimeValueNode(const document::select::CurrentTimeValueNode&) override {
        hasCurrentTime = true;
    }
};

}

bool
DistributorConfiguration::containsTimeStatement(const std::string& documentSelection) const
{
    TimeVisitor visitor;
    try {
        document::select::Parser parser(*_component.getTypeRepo(), _component.getBucketIdFactory());

        std::unique_ptr<document::select::Node> node = parser.parse(documentSelection);
        node->visit(visitor);
    } catch (std::exception& e) {
        LOG(error,
            "Caught exception during config-time processing of GC "
            "selection '%s', terminating process to force full "
            "reconfiguration: %s",
            documentSelection.c_str(),
            e.what());
        std::terminate();
    }
    return visitor.hasCurrentTime;
}

void
DistributorConfiguration::configureMaintenancePriorities(
        const vespa::config::content::core::StorDistributormanagerConfig& cfg)
{
    MaintenancePriorities& mp(_maintenancePriorities);
    mp.mergeMoveToIdealNode = cfg.priorityMergeMoveToIdealNode;
    mp.mergeOutOfSyncCopies = cfg.priorityMergeOutOfSyncCopies;
    mp.mergeTooFewCopies = cfg.priorityMergeTooFewCopies;
    mp.activateNoExistingActive = cfg.priorityActivateNoExistingActive;
    mp.activateWithExistingActive = cfg.priorityActivateWithExistingActive;
    mp.deleteBucketCopy = cfg.priorityDeleteBucketCopy;
    mp.joinBuckets = cfg.priorityJoinBuckets;
    mp.splitDistributionBits = cfg.prioritySplitDistributionBits;
    mp.splitLargeBucket = cfg.prioritySplitLargeBucket;
    mp.splitInconsistentBucket = cfg.prioritySplitInconsistentBucket;
    mp.garbageCollection = cfg.priorityGarbageCollection;
}

void 
DistributorConfiguration::configure(const vespa::config::content::core::StorDistributormanagerConfig& config) 
{
    if ((config.splitsize != 0 && config.joinsize > config.splitsize)
        || (config.splitcount != 0 && config.joincount > config.splitcount))
    {
        std::ostringstream ost;
        ost << "Split limits must be higher than join limits (both count and "
            << "size). Values gotten are size(join(" << config.joinsize
            << ")/split(" << config.splitsize << ")) count(join("
            << config.joincount << ")/split(" << config.splitcount << "))";
        throw vespalib::IllegalArgumentException(ost.str(), VESPA_STRLOC);
    }
    
    _maxIdealStateOperations = config.maxpendingidealstateoperations;
    _byteCountSplitLimit = config.splitsize;
    _docCountSplitLimit = config.splitcount;
    _byteCountJoinLimit = config.joinsize;
    _docCountJoinLimit = config.joincount;
    _minimalBucketSplit = config.minsplitcount;
    _maxNodesPerMerge = config.maximumNodesPerMerge;

    _garbageCollectionInterval = config.garbagecollection.interval;

    if (containsTimeStatement(config.garbagecollection.selectiontoremove)) {
        // Always changes.
        _lastGarbageCollectionChange = 1;
    } else if (_garbageCollectionSelection != config.garbagecollection.selectiontoremove) {
        _lastGarbageCollectionChange = duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
    }

    _garbageCollectionSelection = config.garbagecollection.selectiontoremove;

    // Don't garbage collect with empty selection.
    if (_garbageCollectionSelection.empty()) {
      _garbageCollectionInterval = 0;
    }

    _blockedStateCheckers.clear();
    for (uint32_t i = 0; i < config.blockedstatecheckers.size(); ++i) {
        _blockedStateCheckers.insert(config.blockedstatecheckers[i]);
    }

    _doInlineSplit = config.inlinebucketsplitting;
    _enableJoinForSiblingLessBuckets = config.enableJoinForSiblingLessBuckets;
    _enableInconsistentJoin = config.enableInconsistentJoin;

    _enableHostInfoReporting = config.enableHostInfoReporting;
    _disableBucketActivation = config.disableBucketActivation;
    _sequenceMutatingOperations = config.sequenceMutatingOperations;
    _allowStaleReadsDuringClusterStateTransitions = config.allowStaleReadsDuringClusterStateTransitions;

    _minimumReplicaCountingMode = config.minimumReplicaCountingMode;

    configureMaintenancePriorities(config);

    if (config.maxClusterClockSkewSec >= 0) {
        _maxClusterClockSkew = std::chrono::seconds(config.maxClusterClockSkewSec);
    }
    if (config.inhibitMergeSendingOnBusyNodeDurationSec >= 0) {
        _inhibitMergeSendingOnBusyNodeDuration = std::chrono::seconds(config.inhibitMergeSendingOnBusyNodeDurationSec);
    }
    
    LOG(debug,
        "Distributor now using new configuration parameters. Split limits: %d docs/%d bytes. "
        "Join limits: %d docs/%d bytes. Minimal bucket split %d. "
        "Documents to garbage collect: %s (check every %d seconds). "
        "Maximum pending ideal state operations: %d",
        (int)_docCountSplitLimit,
        (int)_byteCountSplitLimit,
        (int)_docCountJoinLimit,
        (int)_byteCountJoinLimit,
        (int)_minimalBucketSplit,
        _garbageCollectionSelection.c_str(),
        (int)_garbageCollectionInterval,
        (int)_maxIdealStateOperations);
}

void 
DistributorConfiguration::configure(const vespa::config::content::core::StorVisitordispatcherConfig& config)
{
    _minBucketsPerVisitor = config.minbucketspervisitor;
    _maxVisitorsPerNodePerClientVisitor = config.maxvisitorspernodeperclientvisitor;
}

} // storage

