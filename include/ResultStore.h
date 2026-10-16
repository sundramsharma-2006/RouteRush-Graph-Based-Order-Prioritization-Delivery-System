// ===== MEMBER 4 (Ishita Sirohi) : persist the delivery log =====
#pragma once
#include "FileStore.h"
#include "Types.h"

// records.csv : one row per processed order (delivered or rejected)
inline bool saveRecords(const std::vector<DeliveryRecord>& recs, const std::string& path) {
    std::ofstream f(path);
    if (!f) return false;
    f << "orderId,customer,priority,status,note,pickDistance,deliveryDistance,vehicles,cost,"
         "arrivalTick,startTick,completionTick,deadlineTick,late\n";
    for (const DeliveryRecord& r : recs) {
        f << filestore_detail::clean(r.orderId) << ',' << filestore_detail::clean(r.customer) << ','
          << toString(r.priority) << ',' << toString(r.status) << ',' << filestore_detail::clean(r.note) << ','
          << r.pickDistance << ',' << r.deliveryDistance << ',' << r.vehicles << ',' << r.cost << ','
          << r.arrivalTick << ',' << r.startTick << ',' << r.completionTick << ',' << r.deadlineTick << ','
          << (r.late ? 1 : 0) << '\n';
    }
    return static_cast<bool>(f);
}
