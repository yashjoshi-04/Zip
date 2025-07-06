#ifndef _AMERICAN_TRADE
#define _AMERICAN_TRADE

#include <cassert>
#include <string>
#include <algorithm> // For std::max
#include "TreeProduct.h" // Base class
#include "Types.h"       // For OptionType
#include "Payoff.h"      // For PAYOFF namespace
#include "helper.h"      // For to_upper (assuming it's there)


class AmericanOption : public TreeProduct {
public:
	AmericanOption() : optType(OptionType::None), strike(0.0) {}
	AmericanOption(OptionType optType_arg, double notional_arg, double strike_arg, const Date& start_arg, const Date& expiry_arg, const std::string& name_arg)
	{
		this->tradeType = "AmericanOption";
		this->underlying = to_upper(name_arg);
		this->optType = optType_arg;
		this->strike = strike_arg;
		this->expiryDate = expiry_arg;
		this->notional = notional_arg;
		this->tradeDate = start_arg;
		this->rateCurve = "USD-SOFR";
        if (this->underlying.rfind("SGD", 0) == 0 || this->underlying.rfind("STI",0) == 0) {
            this->rateCurve = "SGD-SORA";
        }
	}
    // virtual ~AmericanOption() = default; // Implicitly virtual

	// Trade interface overrides
	std::string getType() const override { return tradeType; }
	std::string getUnderlying() const override { return underlying; }
	double getNotional() const override { return notional; }
    // Pv is inherited from TreeProduct

    // TreeProduct interface implementations
	double Payoff(double S) const override
	{
		return PAYOFF::VanillaOption(optType, strike, S);
	}
	const Date& GetExpiry() const override
	{
		return expiryDate;
	}
	double ValueAtNode(double S, double t, double continuation) const override
	{
		return std::max(Payoff(S), continuation);
	}

    OptionType getOptType() const override { return optType; }
    double getStrike() const override { return strike; }
    const std::string& getRateCurve() const override { return rateCurve; }

protected:
	OptionType optType;
	double strike;
	Date expiryDate;
    std::string rateCurve;
};

class AmerCallSpread : public AmericanOption {
public:
	AmerCallSpread(const std::string& name_arg, double notional_arg, double k1, double k2, const Date& start_arg, const Date& expiry_arg)
	{
        this->tradeType = "AmerCallSpread";
        this->underlying = to_upper(name_arg);
        this->optType = OptionType::Call;
        this->strike = k1; // Base class strike
        this->strike1_val = k1;
        this->strike2_val = k2;
        this->expiryDate = expiry_arg;
        this->notional = notional_arg;
        this->tradeDate = start_arg;
		this->rateCurve = "USD-SOFR";
        if (this->underlying.rfind("SGD", 0) == 0 || this->underlying.rfind("STI",0) == 0) {
             this->rateCurve = "SGD-SORA";
        }
		assert(k1 < k2);
	};

	double Payoff(double S) const override
	{
		return PAYOFF::CallSpread(strike1_val, strike2_val, S);
	}
	// Inherits GetExpiry, ValueAtNode, getOptType, getRateCurve from AmericanOption.
    // getStrike() will return k1. ValueAtNode will use the spread's Payoff.

private:
	double strike1_val;
	double strike2_val;
};

#endif
