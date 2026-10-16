// ===== MEMBER 1 (Sundram Sharma) : vehicle hierarchy (polymorphism) =====
// Each vehicle tracks the tick at which it becomes free again (used by Fleet for scheduling).
#pragma once
#include <string>

class Vehicle {
public:
    explicit Vehicle(std::string id) : id_(std::move(id)) {}
    virtual ~Vehicle() = default;
    virtual std::string type() const = 0;
    virtual double capacityKg() const = 0;
    virtual double costPerKm() const = 0;
    bool canCarry(double kg) const { return kg <= capacityKg(); }
    double tripCost(double km) const { return km * costPerKm(); }
    const std::string& id() const { return id_; }
    long freeAt() const { return freeAt_; }
    void setFreeAt(long t) { freeAt_ = t; }
private:
    std::string id_;
    long freeAt_ = 0;
};

class Bike  : public Vehicle {
public:
    using Vehicle::Vehicle;
    std::string type() const override { return "Bike"; }
    double capacityKg() const override { return 10.0; }
    double costPerKm() const override { return 2.0; }
};
class Van   : public Vehicle {
public:
    using Vehicle::Vehicle;
    std::string type() const override { return "Van"; }
    double capacityKg() const override { return 500.0; }
    double costPerKm() const override { return 8.0; }
};
class Truck : public Vehicle {
public:
    using Vehicle::Vehicle;
    std::string type() const override { return "Truck"; }
    double capacityKg() const override { return 5000.0; }
    double costPerKm() const override { return 20.0; }
};
