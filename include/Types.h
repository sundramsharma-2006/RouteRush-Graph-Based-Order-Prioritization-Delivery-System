// ===== MEMBER 1 (Sundram Sharma) : core domain types =====
// Plain data structs shared by every module. No logic here on purpose.
#pragma once
#include <string>
#include <vector>

enum class Priority { NORMAL = 1, EXPRESS = 2, URGENT = 3 };
enum class OrderStatus { PLACED, QUEUED, PICKING, PACKED, DISPATCHED, DELIVERED, REJECTED };

struct Product {
    std::string id;
    std::string name;
    int quantity = 0;        // units currently available
    int node = 0;            // graph node (storage zone) where the product sits
    double weightKg = 0.0;   // per unit
};

struct OrderLine {
    std::string productId;
    int qty = 0;
};

struct Order {
    std::string id;
    std::string customer;
    int destNode = 0;                 // delivery node in the graph
    Priority priority = Priority::NORMAL;
    long arrivalTick = 0;             // logical clock, used for aging
    long deadlineTick = -1;           // absolute tick by which delivery should complete (-1 = none)
    OrderStatus status = OrderStatus::PLACED;
    std::vector<OrderLine> lines;
};

inline std::string toString(Priority p) {
    switch (p) {
        case Priority::URGENT:  return "URGENT";
        case Priority::EXPRESS: return "EXPRESS";
        default:                return "NORMAL";
    }
}
inline bool parsePriority(const std::string& s, Priority& out) {
    if (s == "URGENT")  { out = Priority::URGENT;  return true; }
    if (s == "EXPRESS") { out = Priority::EXPRESS; return true; }
    if (s == "NORMAL")  { out = Priority::NORMAL;  return true; }
    return false;
}

inline std::string toString(OrderStatus s) {
    static const char* names[] = {"PLACED","QUEUED","PICKING","PACKED","DISPATCHED","DELIVERED","REJECTED"};
    return names[static_cast<int>(s)];
}
inline bool parseStatus(const std::string& s, OrderStatus& out) {
    for (int i = 0; i <= static_cast<int>(OrderStatus::REJECTED); ++i) {
        if (toString(static_cast<OrderStatus>(i)) == s) { out = static_cast<OrderStatus>(i); return true; }
    }
    return false;
}

// One row of the delivery log: what happened to an order.
struct DeliveryRecord {
    std::string orderId;
    std::string customer;
    Priority priority = Priority::NORMAL;
    OrderStatus status = OrderStatus::PLACED;   // DELIVERED or REJECTED
    std::string note;                           // rejection reason (empty if delivered)
    double pickDistance = 0.0;                  // depot -> shelves -> packing
    double deliveryDistance = 0.0;              // packing -> customer (one way)
    std::string vehicles;                       // e.g. "Van:V1+Bike:B2"
    double cost = 0.0;                          // round-trip delivery cost
    long arrivalTick = 0;
    long startTick = 0;                         // picking started
    long completionTick = 0;                    // delivered
    long deadlineTick = -1;
    bool late = false;
};
