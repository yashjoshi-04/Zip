#include "Market.h"

using namespace std;

namespace imp {
	// x0 < x < x1
	double linearInterpolate(double x0, double y0, double x1, double y1, double x) 
	{
		if (x < x0)
			return y0;
		else if (x > x1)
			return y1;
		else		
			return y0 + (x - x0) * (y1 - y0) / (x1 - x0);
	}
}

void RateCurve::display() const {
	cout << "rate curve:" << name << endl;
	for (size_t i = 0; i < tenors.size(); i++) {
		cout << tenors[i] << ":" << rates[i] << endl;
	}
	cout << endl;
}
void RateCurve::addRate(Date tenor, double rate) {
	//consider to check if tenor already exist
	if (find(tenors.begin(), tenors.end(), tenor) == tenors.end()) {
		tenors.push_back(tenor);
		rates.push_back(rate);
	}
}
double RateCurve::getRate(Date date) const 
{
	//use linear interpolation to get rate	
	auto it = std::lower_bound(tenors.begin(), tenors.end(), date);
	if (it == tenors.end()) // cannot find any item which is >= value
		return *rates.end();
	if (it == tenors.begin())
		return *rates.begin();
	Date dt = *it;
	if (dt == date)
		return rates[it - tenors.begin()];
	else {
		auto it0 = it;
		it0--;
		double x0 = it0->getSerialDate();
		double y0 = rates[it0 - tenors.begin()];
		double x1 = it->getSerialDate();
		double y1 = rates[it - tenors.begin()];
		double x = date.getSerialDate();
		double newRate = imp::linearInterpolate(x0, y0, x1, y1, x);
		return newRate;
	}
}
double RateCurve::getDf(Date _date) const
{
	double ccr = getRate(_date);
	double t = (_date - _asOf)/365.0;
	return exp(-ccr * t);
}
void RateCurve::shock(Date tenor, double value) 
{
	// parallel shock all tenors rate
	for (auto& rt : rates) {
		rt += value;
	}
}

void VolCurve::addVol(Date tenor, double vol) 
{
	//consider to check if tenor already exist
	if (find(tenors.begin(), tenors.end(), tenor) == tenors.end()) {
		tenors.push_back(tenor);
		vols.push_back(vol);
	}
}
double VolCurve::getVol(Date date) const
{
	//use linear interpolation to get rate	
	auto it = std::lower_bound(tenors.begin(), tenors.end(), date);
	if (it == tenors.end()) // cannot find any item which is >= value
		return *vols.end();
	if (it == tenors.begin())
		return *vols.begin();
	Date dt = *it;
	if (dt == date)
		return vols[it - tenors.begin()];
	else {
		auto it0 = it;
		it0--;
		double x0 = it0->getSerialDate();
		double y0 = vols[it0 - tenors.begin()];
		double x1 = it->getSerialDate();
		double y1 = vols[it - tenors.begin()];
		double x = date.getSerialDate();
		double vol = imp::linearInterpolate(x0, y0, x1, y1, x);
		return vol;
	}
}
void VolCurve::display() const 
{
	cout << "vol curve:" << name << endl;
	for (size_t i = 0; i < tenors.size(); i++) {
		cout << tenors[i] << ":" << vols[i] << endl;
	}
	cout << endl;
}
void VolCurve::shock(Date tenor, double value)
{
	// parallel shock all tenors rate
	for (auto& v : vols) {
		v += value;
	}
}

void Market::Print() const
{
	cout << "market asof: " << asOf << endl;

	for (auto curve : curves) {
		curve.second->display();
	}
	for (auto vol : vols) {
		vol.second->display();
	}
	for (auto price : bondPrices) {
		cout << "Bond: " << price.first << ", Price: " << price.second << endl;
	}
	for (auto price : stockPrices) {
		cout << "Stock: " << price.first << ", Price: " << price.second << endl;
	}
}
void Market::addCurve(const std::string& name, shared_ptr<RateCurve> curve) 
{
	curves.emplace(name, curve);
}
void Market::addVolCurve(const std::string& name, shared_ptr<VolCurve> vol)
{
	vols.emplace(name, vol);
}
void Market::addBondPrice(const std::string& bondName, double price)
{
	bondPrices.emplace(bondName, price);
}
void Market::addStockPrice(const std::string& stockName, double price)
{
	stockPrices.emplace(stockName, price);
}
std::ostream& operator<<(std::ostream& os, const Market& mkt)
{
	os << mkt.asOf << std::endl;
	return os;
}
std::istream& operator>>(std::istream& is, Market& mkt)
{
	is >> mkt.asOf;
	return is;
}
