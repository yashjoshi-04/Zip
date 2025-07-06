#include "Market.h"
#include <algorithm>
#include <stdexcept>
#include <cmath>
#include <iostream> // For std::cout

using namespace std; // Be cautious with 'using namespace std;' in .cpp files if they include many headers

namespace imp {
	double linearInterpolate(double x0, double y0, double x1, double y1, double x)
	{
		if (std::abs(x1 - x0) < 1e-9) {
			return y0;
		}
		if (x <= x0)
			return y0;
		else if (x >= x1)
			return y1;
		else
			return y0 + (x - x0) * (y1 - y0) / (x1 - x0);
	}
}

// --- RateCurve Methods ---
void RateCurve::display() const {
	cout << "Rate Curve: " << name << " (AsOf: " << _asOf << ")" << endl;
	for (size_t i = 0; i < tenors.size(); i++) {
		cout << "  " << tenors[i] << ": " << rates[i] * 100 << "%" << endl;
	}
	cout << endl;
}

void RateCurve::addRate(Date tenor, double rate) {
    auto it_tenor = std::lower_bound(tenors.begin(), tenors.end(), tenor);
    size_t insert_idx = it_tenor - tenors.begin();

    if (it_tenor != tenors.end() && *it_tenor == tenor) {
        rates[insert_idx] = rate;
    } else {
        tenors.insert(it_tenor, tenor);
        rates.insert(rates.begin() + insert_idx, rate);
    }
}

double RateCurve::getRate(Date date) const {
    if (tenors.empty()) {
        throw std::runtime_error("Rate curve '" + name + "' is empty. Cannot get rate.");
    }
    if (tenors.size() == 1) {
        return rates[0];
    }

    auto it = std::lower_bound(tenors.begin(), tenors.end(), date);

    double x_serial = date.getSerialDate();
    double x0_serial, y0_rate, x1_serial, y1_rate;

    if (it == tenors.begin()) {
        x0_serial = tenors[0].getSerialDate();
        y0_rate = rates[0];
        x1_serial = tenors[1].getSerialDate();
        y1_rate = rates[1];
    } else if (it == tenors.end()) {
        x1_serial = tenors.back().getSerialDate();
        y1_rate = rates.back();
        x0_serial = tenors[tenors.size() - 2].getSerialDate();
        y0_rate = rates[rates.size() - 2];
    } else {
        x1_serial = it->getSerialDate();
        y1_rate = rates[it - tenors.begin()];
        x0_serial = (it - 1)->getSerialDate();
        y0_rate = rates[(it - 1) - tenors.begin()];
    }
    return imp::linearInterpolate(x0_serial, y0_rate, x1_serial, y1_rate, x_serial);
}

double RateCurve::getDf(Date targetDate) const
{
    if (_asOf.getSerialDate() == 0 && !(_asOf.year == 0 && _asOf.month == 0 && _asOf.day == 0) ) { // Allow uninitialized Date() to be 0,0,0
         // A more robust check for an uninitialized _asOf might be needed if Date() default constructor doesn't set to an invalid serial.
         // For now, assume if serial is 0 and it's not explicitly {0,0,0} it's an issue.
         // A better way: add an isValid() method to Date or ensure _asOf is always set.
    }
	double ccr = getRate(targetDate);
	double t_years = (targetDate - _asOf) / 365.0;
    if (targetDate < _asOf) { // If targetDate is strictly before asOf, T is negative.
        t_years = 0; // For DF calculation, effectively making DF=1 for past dates if needed for some formula.
                     // Or, this situation implies a past cash flow, which should be handled by caller.
    }
	return std::exp(-ccr * t_years);
}

void RateCurve::shock(Date tenor_date_ignored, double shock_value)
{
	for (auto& rt_val : rates) {
		rt_val += shock_value;
	}
}

// --- VolCurve Methods ---
void VolCurve::display() const {
	cout << "Volatility Curve: " << name << " (AsOf: " << _asOf << ")" << endl;
	for (size_t i = 0; i < tenors.size(); i++) {
		cout << "  " << tenors[i] << ": " << vols[i] * 100 << "%" << endl;
	}
	cout << endl;
}

void VolCurve::addVol(Date tenor, double vol)
{
    auto it_tenor = std::lower_bound(tenors.begin(), tenors.end(), tenor);
    size_t insert_idx = it_tenor - tenors.begin();

    if (it_tenor != tenors.end() && *it_tenor == tenor) {
        vols[insert_idx] = vol;
    } else {
        tenors.insert(it_tenor, tenor);
        vols.insert(vols.begin() + insert_idx, vol);
    }
}

double VolCurve::getVol(Date date) const
{
    if (tenors.empty()) {
        throw std::runtime_error("Volatility curve '" + name + "' is empty. Cannot get volatility.");
    }
    if (tenors.size() == 1) {
        return vols[0];
    }

    auto it = std::lower_bound(tenors.begin(), tenors.end(), date);
    double x_serial = date.getSerialDate();
    double x0_serial, y0_vol, x1_serial, y1_vol;

    if (it == tenors.begin()) {
        x0_serial = tenors[0].getSerialDate();
        y0_vol = vols[0];
        x1_serial = tenors[1].getSerialDate();
        y1_vol = vols[1];
    } else if (it == tenors.end()) {
        x1_serial = tenors.back().getSerialDate();
        y1_vol = vols.back();
        x0_serial = tenors[tenors.size() - 2].getSerialDate();
        y0_vol = vols[vols.size() - 2];
    } else {
        x1_serial = it->getSerialDate();
        y1_vol = vols[it - tenors.begin()];
        x0_serial = (it - 1)->getSerialDate();
        y0_vol = vols[(it - 1) - tenors.begin()];
    }
    return imp::linearInterpolate(x0_serial, y0_vol, x1_serial, y1_vol, x_serial);
}

void VolCurve::shock(Date tenor_date_ignored, double shock_value)
{
	for (auto& v_val : vols) {
		v_val += shock_value;
	}
}

// --- Market Methods ---
void Market::Print() const
{
	cout << "Market AsOf Date: " << asOf << endl;
    cout << "------------------------------------" << endl;
	for (auto const& pair_item : curves) { // Changed to avoid C++17 structured binding if it causes issues
		pair_item.second->display();
	}
	for (auto const& pair_item : vols) {
		pair_item.second->display();
	}
    cout << "Stock Prices:" << endl;
	for (auto const& pair_item : stockPrices) {
		cout << "  " << pair_item.first << ": " << pair_item.second << endl;
	}
    cout << "------------------------------------" << endl << endl;
}

void Market::addCurve(const std::string& name, shared_ptr<RateCurve> curve_ptr)
{
	curves.emplace(name, curve_ptr);
}

void Market::addVolCurve(const std::string& name, shared_ptr<VolCurve> vol_ptr)
{
	vols.emplace(name, vol_ptr);
}

void Market::addBondPrice(const std::string& bondName, double price)
{
	bondPrices.emplace(bondName, price);
}

void Market::addStockPrice(const std::string& stockName, double price)
{
	stockPrices[stockName] = price;
}

std::ostream& operator<<(std::ostream& os, const Market& mkt_obj)
{
    // Simplified to avoid accessing private members directly from a non-friend non-member function
	os << "Market(asOf=" << mkt_obj.asOf << ")";
	return os;
}

std::istream& operator>>(std::istream& is, Market& mkt_obj)
{
	is >> mkt_obj.asOf;
	return is;
}
```

**Step 2: Fix `RiskEngine.h` (remove ```, check includes)**
