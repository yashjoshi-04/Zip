#ifndef _AMERICAN_TRADE
#define _AMERICAN_TRADE

#include <cassert> 

#include "TreeProduct.h"
#include "Types.h"
#include "Payoff.h"

class AmericanOption : public TreeProduct {
public:
	AmericanOption() {}
	AmericanOption(OptionType _optType, double _notional, double _strike, const Date& _start, const Date& _expiry, const std::string& name)
	{
		tradeType = "TreeProduct";
		underlying = to_upper(name);
		optType = _optType;
		strike = _strike;
		expiryDate = _expiry;
		notional = _notional;
		tradeDate = _start;		
		rateCurve = "USD-SOFR"; // default rate curve, can be changed later
	}
	inline string getType() const { return tradeType; };
	inline string getUnderlying() const { return underlying; };
	inline double getNotional() const { return notional; }
	virtual double Payoff(double S) const
	{
		return PAYOFF::VanillaOption(optType, strike, S);
	}
	virtual const Date& GetExpiry() const
	{
		return expiryDate;
	}
	virtual double ValueAtNode(double S, double t, double continuation) const
	{
		return std::max(Payoff(S), continuation);
	}

private:
	OptionType optType;
	double strike;
	Date expiryDate;
	string rateCurve;
};

class AmerCallSpread : public TreeProduct {
public:
	AmerCallSpread(double _k1, double _k2, const Date& _expiry)
		: strike1(_k1), strike2(_k2), expiryDate(_expiry)
	{
		assert(_k1 < _k2);
	};
	virtual double Payoff(double S) const
	{
		return PAYOFF::CallSpread(strike1, strike2, S);
	}
	virtual const Date& GetExpiry() const
	{
		return expiryDate;
	}

private:
	double strike1;
	double strike2;
	Date expiryDate;
};

#endif
