#pragma once
#include "Trade.h"
#include "helper.h"

class Swap : public Trade {
public:
	//make necessary change
	Swap(string name, Date start, Date end, double _notional, double _rate, double _freq)
	{
		tradeType = "Swap";
		underlying = to_upper(name);
		startDate = start;
		maturityDate = end;
		tradeDate = start;
		notional = _notional;
		tradeRate = _rate;
		frequency = _freq; 
		rateCurve = to_upper(name).substr(0, 3) == "SGD" ? "SGD-SORA" : "USD-SOFR";
		generateSchedule();
	}

	/*
	implement this, using npv = discounted cash flow from both leg;
	*/
	inline string getType() const { return tradeType; };
	inline string getUnderlying() const { return underlying; };
	inline double getNotional() const { return notional; }
	double Payoff(double r) const;
	double Pv(const Market& mkt) const;
	double getAnnuity(const Market& mkt) const; //implement this in a cpp file
	void generateSchedule();
	

private:
	Date startDate;
	Date maturityDate;
	double tradeRate; // fixed leg rate
	double frequency; // use 1 for annual, 2 for semi-annual etc
	vector<Date> swapSchedule;
	string rateCurve;

};