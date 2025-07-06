#ifndef MARKET_H
#define MARKET_H

#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <stdexcept> // For std::runtime_error
#include "Date.h"

class RateCurve {
public:
	RateCurve() : _asOf(Date(0,0,0)) {};
	RateCurve(const std::string& name_arg) : name(name_arg), _asOf(Date(0,0,0)) {};
    RateCurve(const RateCurve& other) = default;
    RateCurve& operator=(const RateCurve& other) = default;
    virtual ~RateCurve() = default;

	void addRate(Date tenor, double rate);
	void shock(Date tenor_date_ignored, double shock_value);
	double getRate(Date date) const;
	double getDf(Date date) const;
	void display() const;

	std::string name;
	Date _asOf;

private:
	std::vector<Date> tenors;
	std::vector<double> rates;
};

class VolCurve {
public:
	VolCurve() : _asOf(Date(0,0,0)) {}
	VolCurve(const std::string& name_arg) : name(name_arg), _asOf(Date(0,0,0)) {};
    VolCurve(const VolCurve& other) = default;
    VolCurve& operator=(const VolCurve& other) = default;
    virtual ~VolCurve() = default;

	void addVol(Date tenor, double vol);
	double getVol(Date date) const;
	void display() const;
	void shock(Date tenor_date_ignored, double shock_value);

	std::string name;
	Date _asOf;

private:
	std::vector<Date> tenors;
	std::vector<double> vols;
};

class Market
{
public:
	Date asOf;
	Market() : asOf(Date(0,0,0)) {
	};
	Market(const Date& now) : asOf(now) {};
	Market(const Market& other) {
		this->asOf = other.asOf;
		for (const auto& curve_pair : other.curves) {
			curves.emplace(curve_pair.first, std::make_shared<RateCurve>(*(curve_pair.second)));
		}
		for (const auto& vol_pair : other.vols) {
			vols.emplace(vol_pair.first, std::make_shared<VolCurve>(*(vol_pair.second)));
		}
		bondPrices = other.bondPrices;
		stockPrices = other.stockPrices;
	}
    Market& operator=(const Market& other) {
        if (this == &other) return *this;
        this->asOf = other.asOf;
        curves.clear();
        for (const auto& curve_pair : other.curves) {
			curves.emplace(curve_pair.first, std::make_shared<RateCurve>(*(curve_pair.second)));
		}
        vols.clear();
		for (const auto& vol_pair : other.vols) {
			vols.emplace(vol_pair.first, std::make_shared<VolCurve>(*(vol_pair.second)));
		}
		bondPrices = other.bondPrices;
		stockPrices = other.stockPrices;
        return *this;
    }
    virtual ~Market() = default;

	void Print() const;
	void addCurve(const std::string& name, std::shared_ptr<RateCurve> curve);
	void addVolCurve(const std::string& name, std::shared_ptr<VolCurve> vol);
	void addBondPrice(const std::string& bondName, double price);
	void addStockPrice(const std::string& stockName, double price);

	inline void shockPrice(const std::string& underlying, double shock_amount) {
        auto it = stockPrices.find(underlying);
        if (it != stockPrices.end()) {
            it->second += shock_amount;
        }
    }
	inline std::shared_ptr<RateCurve> getCurve(const std::string& name) const {
        auto it = curves.find(name);
        return (it != curves.end()) ? it->second : nullptr;
    }
    inline bool hasCurve(const std::string& name) const {
        return curves.count(name) > 0;
    }
	inline std::shared_ptr<VolCurve> getVolCurve(const std::string& name) const {
        auto it = vols.find(name);
        return (it != vols.end()) ? it->second : nullptr;
    }
    inline bool hasVolCurve(const std::string& name) const {
        return vols.count(name) > 0;
    }
	inline double getStockPrice(const std::string& name) const {
		auto it = stockPrices.find(name);
		if (it != stockPrices.end()) {
			return it->second;
		}
		throw std::runtime_error("Stock price not found for: " + name);
	}
    inline bool hasStockPrice(const std::string& name) const {
        return stockPrices.count(name) > 0;
    }

private:
	std::unordered_map<std::string, std::shared_ptr<VolCurve>> vols;
	std::unordered_map<std::string, std::shared_ptr<RateCurve>> curves;
	std::unordered_map<std::string, double> bondPrices;
	std::unordered_map<std::string, double> stockPrices;
};

std::ostream& operator<<(std::ostream& os, const Market& obj);
std::istream& operator>>(std::istream& is, Market& obj);

#endif
