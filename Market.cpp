#include "Market.h"
#include <algorithm> // Required for std::lower_bound, std::find (if used)
#include <stdexcept> // Required for std::runtime_error
#include <cmath>     // For std::exp, std::abs

using namespace std;

namespace imp {
	// x0 <= x <= x1 for interpolation
	// x < x0 for y0 extrapolation, x > x1 for y1 extrapolation
	double linearInterpolate(double x0, double y0, double x1, double y1, double x)
	{
		if (std::abs(x1 - x0) < 1e-9) { // Avoid division by zero if points are (almost) identical X
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
    // Insert new tenor and rate while maintaining sorted order by tenor date.
    // std::lower_bound returns an iterator to the first element not less than 'tenor'.
    auto it_tenor = std::lower_bound(tenors.begin(), tenors.end(), tenor);
    size_t insert_idx = it_tenor - tenors.begin();

    if (it_tenor != tenors.end() && *it_tenor == tenor) { // Tenor already exists
        rates[insert_idx] = rate; // Update the rate for the existing tenor
    } else { // New tenor, insert to maintain sorted order
        tenors.insert(it_tenor, tenor);
        rates.insert(rates.begin() + insert_idx, rate);
    }
}

double RateCurve::getRate(Date date) const {
    if (tenors.empty()) {
        throw std::runtime_error("Rate curve '" + name + "' is empty. Cannot get rate.");
    }
    if (tenors.size() == 1) {
        return rates[0]; // Flat rate if only one point in the curve
    }

    auto it = std::lower_bound(tenors.begin(), tenors.end(), date);

    double x_serial = date.getSerialDate();
    double x0_serial, y0_rate, x1_serial, y1_rate;

    if (it == tenors.begin()) {
        // Date is before or at the first tenor point.
        x0_serial = tenors[0].getSerialDate();
        y0_rate = rates[0];
        x1_serial = tenors[1].getSerialDate();
        y1_rate = rates[1];
    } else if (it == tenors.end()) {
        // Date is after the last tenor point.
        x1_serial = tenors.back().getSerialDate();
        y1_rate = rates.back();
        x0_serial = tenors[tenors.size() - 2].getSerialDate();
        y0_rate = rates[rates.size() - 2];
    } else {
        // Date is between two points or exactly on a point *it.
        x1_serial = it->getSerialDate();
        y1_rate = rates[it - tenors.begin()];
        x0_serial = (it - 1)->getSerialDate();
        y0_rate = rates[(it - 1) - tenors.begin()];
    }
    return imp::linearInterpolate(x0_serial, y0_rate, x1_serial, y1_rate, x_serial);
}

double RateCurve::getDf(Date targetDate) const
{
    if (_asOf.getSerialDate() == 0) { // Check if _asOf is initialized
         throw std::runtime_error("RateCurve " + name + " _asOf date is not initialized.");
    }
	double ccr = getRate(targetDate); // Zero Coupon Rate (continuously compounded)

    double t_days = (targetDate - _asOf);
    if (targetDate < _asOf) { // Discount factor for a past date is typically not well-defined this way for PV.
                              // For PV purposes, cash flows before asOf are ignored.
                              // If targetDate is before asOf, T would be negative.
                              // Let's ensure T is non-negative for exp(-rT).
                              // If targetDate == asOf, T=0, DF=1.
        t_days = 0; // Treat past dates as having T=0 for DF calc, effectively DF=1 if needed, or handle upstream.
                    // More robustly, PV calcs should skip past cashflows.
                    // If targetDate == _asOf, t_days = 0, t_years = 0, exp(0) = 1. Correct.
    }
    if (targetDate < _asOf) return 1.0; // Or handle based on context, often means CF already passed.
                                        // For a generic DF function, this might be debated.
                                        // But in PV context, DF for past is moot.
                                        // Let's assume targetDate >= _asOf for standard DF.
                                        // If _asOf > targetDate, T is negative. exp(-r * -T) = exp(rT) > 1. (compounding)
                                        // This function is for Discount Factor. So T should be time from _asOf to targetDate.

	double t_years = (targetDate - _asOf) / 365.0; // Using fixed 365 days for year fraction
    if (t_years < 0) t_years = 0; // Ensure T is not negative for exp(-rT) if targetDate < _asOf

	return std::exp(-ccr * t_years);
}

void RateCurve::shock(Date tenor_date_ignored, double shock_value)
{
	// Parallel shock: add shock_value to all rates in the curve.
    // tenor_date_ignored is not used for parallel shock.
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
	// Parallel shock: add shock_value to all volatilities.
	for (auto& v_val : vols) {
		v_val += shock_value;
	}
}

// --- Market Methods ---
void Market::Print() const
{
	cout << "Market AsOf Date: " << asOf << endl;
    cout << "------------------------------------" << endl;
	for (auto const& [curve_name, curve_ptr] : curves) {
		curve_ptr->display();
	}
	for (auto const& [vol_name, vol_ptr] : vols) {
		vol_ptr->display();
	}
    cout << "Stock Prices:" << endl;
	for (auto const& [stock_name, stock_price] : stockPrices) {
		cout << "  " << stock_name << ": " << stock_price << endl;
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
    // Not used in current project for PV, but good to have
	bondPrices.emplace(bondName, price);
}

void Market::addStockPrice(const std::string& stockName, double price)
{
	stockPrices[stockName] = price; // Use operator[] to add or update
}

std::ostream& operator<<(std::ostream& os, const Market& mkt_obj)
{
	os << "Market(asOf=" << mkt_obj.asOf << ", "
       << mkt_obj.curves.size() << " rate curves, "
       << mkt_obj.vols.size() << " vol curves, "
       << mkt_obj.stockPrices.size() << " stock prices)";
	return os;
}

// istream operator for Market might be complex if it needs to load all curves/data
// For now, keeping it simple if it was just for asOf date.
std::istream& operator>>(std::istream& is, Market& mkt_obj)
{
	is >> mkt_obj.asOf; // Only reads asOf date, assumes curves etc. are populated separately
	return is;
}
```
