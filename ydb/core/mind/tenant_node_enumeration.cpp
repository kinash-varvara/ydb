#include "tenant_node_enumeration.h"
#include "tenant_pool.h"
#include <ydb/core/base/appdata.h>
#include <ydb/core/base/statestorage.h>
#include <ydb/core/base/path.h>
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>
#include <util/generic/algorithm.h>

namespace NKikimr {

TString MakeTenantNodeEnumerationPath(const TString &tenantName) {
    return "node+" + tenantName;
}

static ui32 ExtractDefaultGroupForPath(const TString &path) {
    auto *domains = AppData()->DomainsInfo.Get();
    const TStringBuf domainName = ExtractDomain(path);
    auto *domainInfo = domains->GetDomainByName(domainName);
    if (domainInfo)
        return domainInfo->DefaultStateStorageGroup;
    else
        return Max<ui32>();
}

class TTenantNodeEnumerationPublisher : public TActorBootstrapped<TTenantNodeEnumerationPublisher> {
    TMap<TString, TActorId> PublishActors;

    void Handle(TEvTenantPool::TEvTenantPoolStatus::TPtr &ev) {
        const auto &record = ev->Get()->Record;

        TMap<TString, TActorId> toRemove;
        toRemove.swap(PublishActors);

        for (auto &x : record.GetSlots()) {
            if (const TString &assigned = x.GetAssignedTenant()) {
                auto it = toRemove.find(assigned);
                if (it != toRemove.end()) {
                    PublishActors.emplace(*it);
                    toRemove.erase(it);
                } else {
                    const TString assignedPath = MakeTenantNodeEnumerationPath(assigned);
                    const ui32 statestorageGroupId = ExtractDefaultGroupForPath(assigned);
                    if (statestorageGroupId == Max<ui32>())
                        continue;

                    const TActorId publishActor = Register(CreateBoardPublishActor(assignedPath, TString(), SelfId(), statestorageGroupId, 0, true));

                    PublishActors.emplace(assigned, publishActor);
                }
            }
        }

        for (auto &xpair : toRemove) {
            Send(xpair.second, new TEvents::TEvPoisonPill());
        }
    }
public:
    static constexpr NKikimrServices::TActivity::EType ActorActivityType() {
        return NKikimrServices::TActivity::TENANT_NODES_ENUMERATION;
    }

    TTenantNodeEnumerationPublisher()
    {}

    void Bootstrap() {
        Send(MakeTenantPoolRootID(), new TEvents::TEvSubscribe());
        Become(&TThis::StateWait);
    }

    STATEFN(StateWait) {
        switch (ev->GetTypeRewrite()) {
            hFunc(TEvTenantPool::TEvTenantPoolStatus, Handle);
        }
    }
};

class TTenantNodeEnumerationLookup : public TActorBootstrapped<TTenantNodeEnumerationLookup> {
    const TActorId ReplyTo;
    const TString TenantName;
    TActorId LookupActor;

    void PassAway() override {
        if (LookupActor) {
            Send(LookupActor, new TEvents::TEvPoisonPill());
            LookupActor = TActorId();
        }

        IActor::PassAway();
    }

    void ReportErrorAndDie() {
        Send(ReplyTo, new TEvTenantNodeEnumerator::TEvLookupResult(TenantName, false));
        PassAway();
    }

    void Handle(TEvStateStorage::TEvBoardInfo::TPtr &ev) {
        LookupActor = TActorId();

        auto *msg = ev->Get();

        if (msg->Status != TEvStateStorage::TEvBoardInfo::EStatus::Ok)
            return ReportErrorAndDie();

        TVector<ui32> nodes;
        for (auto &xpair : msg->InfoEntries) {
            nodes.push_back(xpair.first.NodeId());
        }
        SortUnique(nodes);

        Send(ReplyTo, new TEvTenantNodeEnumerator::TEvLookupResult(TenantName, std::move(nodes)));
        PassAway();
    }
public:
    static constexpr NKikimrServices::TActivity::EType ActorActivityType() {
        return NKikimrServices::TActivity::TENANT_NODES_ENUMERATION;
    }

    TTenantNodeEnumerationLookup(TActorId replyTo, const TString &tenantName)
        : ReplyTo(replyTo)
        , TenantName(tenantName)
    {}

    void Bootstrap() {
        const ui32 statestorageGroupId = ExtractDefaultGroupForPath(TenantName);
        if (statestorageGroupId == Max<ui32>())
            return ReportErrorAndDie();

        const TString path = MakeTenantNodeEnumerationPath(TenantName);
        LookupActor = Register(CreateBoardLookupActor(path, SelfId(), statestorageGroupId, EBoardLookupMode::Majority, false, false));

        Become(&TThis::StateWait);
    }

    STATEFN(StateWait) {
        switch (ev->GetTypeRewrite()) {
            hFunc(TEvStateStorage::TEvBoardInfo, Handle);
        }
    }
};

IActor* CreateTenantNodeEnumerationPublisher() {
    return new TTenantNodeEnumerationPublisher();
}

IActor* CreateTenantNodeEnumerationLookup(TActorId replyTo, const TString &tenantName) {
    return new TTenantNodeEnumerationLookup(replyTo, tenantName);
}

}
