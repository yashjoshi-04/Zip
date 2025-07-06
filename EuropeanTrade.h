#ifndef _EUROPEAN_TRADE
#define _EUROPEAN_TRADE

#include <cassert> 

#include "TreeProduct.h"
#include "Payoff.h"
#include "Types.h"

class EuropeanOption : public TreeProduct {
public:
	EuropeanOption() {};
	EuropeanOption(OptionType _optType, double _notional, double _strike, const Date& _start, const Date& _expiry, const std::string& name)
	{
		tradeType = "TreeProduct";
		underlying = to_upper(name);
		optType = _optType;
		strike = _strike;
		expiryDate = _expiry;
		notional = _notional;
		tradeDate = _start;
		rateCurve = "USD-SOFR"; // default rate curve, can be changed later
	};
	inline string getType() const { return tradeType; };
	inline string getUnderlying() const { return underlying; };
	inline double getNotional() const { return notional; }
	virtual double Payoff(double S) const { return PAYOFF::VanillaOption(optType, strike, S); }
	virtual const Date& GetExpiry() const { return expiryDate; }
	virtual double ValueAtNode(double S, double t, double continuation) const { return continuation; }
	
protected:
	OptionType optType;
	double strike = 0;
	Date expiryDate;
	string rateCurve;

};

class EuroCallSpread : public EuropeanOption {
public:
	EuroCallSpread(double _k1, double _k2, const Date& _expiry) : strike1(_k1), strike2(_k2) {
		expiryDate = _expiry;
		assert(_k1 < _k2);
	};
	virtual double Payoff(double S) const { return PAYOFF::CallSpread(strike1, strike2, S); };
	virtual const Date& GetExpiry() const { return expiryDate; };

private:
	double strike1;
	double strike2;
};

#endif
