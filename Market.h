#ifndef MARKET_H
#define MARKET_H

#include <iostream>
#include <vector>
#include <unordered_map>
#include <memory>
#include "Date.h"

using namespace std;

class RateCurve {
public:
	RateCurve() {};
	RateCurve(const string& _name) : name(_name) {};
	void addRate(Date tenor, double rate);
	void shock(Date tenor, double value); //implement this
	double getRate(Date date) const; //implement this function using linear interpolation
	double getDf(Date date) const; // using df = exp(-rt), and r is getRate function
	void display() const;

	std::string name;
	Date _asOf;//same as market data date

private:
	vector<Date> tenors;
	vector<double> rates; //zero coupon rate or continous compounding rate
};

class VolCurve { // atm vol curve without smile
public:
	VolCurve() {}
	VolCurve(const string& _name) : name(_name) {};
	void addVol(Date tenor, double rate); //implement this
	double getVol(Date date) const; //implement this function using linear interpolation
	void display() const; //implement this
	void shock(Date tenor, double value); //implement this

	string name;
	Date _asOf;

private:
	vector<Date> tenors;
	vector<double> vols;
};

class Market
{
public:
	Date asOf;
	Market() {
		cout << "default constructor is called" << endl;
	};
	Market(const Date& now) : asOf(now) {};
	Market(const Market& other) {
		this->asOf = other.asOf;
		// Deep copy each Curve
		for (const auto& curve : other.curves) {
			curves.emplace(curve.first,std::make_shared<RateCurve>(*(curve.second))); // Deep copy each Curve
		}
		// Deep copy each Curve
		for (const auto& vol : other.vols) {
			vols.emplace(vol.first, std::make_shared<VolCurve>(*vol.second)); // Deep copy each Curve
		}
		bondPrices = other.bondPrices;
		stockPrices = other.stockPrices;		
	} 
	void Print() const;
	void addCurve(const std::string& name, shared_ptr<RateCurve> curve);//implement this
	void addVolCurve(const std::string& name, shared_ptr<VolCurve> vol);//implement this
	void addBondPrice(const std::string& bondName, double price);//implement this
	void addStockPrice(const std::string& stockName, double price);//implement this

	inline void shockPrice(const string& underlying, double shock) { stockPrices[underlying] += shock; }
	inline shared_ptr<RateCurve> getCurve(const string& name) const { return curves.at(name); };
	inline shared_ptr<VolCurve> getVolCurve(const string& name) const { return vols.at(name); };
	inline double getStockPrice(const string& name) const {
		auto it = stockPrices.find(name);
		if (it != stockPrices.end()) {
			return it->second;
		}
		throw std::runtime_error("Stock price not found for: " + name);
	}

private:
	unordered_map<string, shared_ptr<VolCurve>> vols;
	unordered_map<string, shared_ptr<RateCurve>> curves;
	unordered_map<string, double> bondPrices;
	unordered_map<string, double> stockPrices;

};

std::ostream& operator<<(std::ostream& os, const Market& obj);
std::istream& operator>>(std::istream& is, Market& obj);

#endif
