#ifndef _AMERICAN_TRADE
#define _AMERICAN_TRADE

#include <cassert>
#include <string> // For std::string
#include "TreeProduct.h"
#include "Types.h"      // For OptionType
#include "Payoff.h"     // For PAYOFF::VanillaOption etc.

class AmericanOption : public TreeProduct {
public:
	AmericanOption() {} // Default constructor
	AmericanOption(OptionType optType_arg, double notional_arg, double strike_arg, const Date& start_arg, const Date& expiry_arg, const std::string& name_arg)
	{
		tradeType = "AmericanOption"; // More specific type
		underlying = to_upper(name_arg); // Assuming to_upper is in global scope or helper
		optType = optType_arg;
		strike = strike_arg;
		expiryDate = expiry_arg;
		notional = notional_arg;
		tradeDate = start_arg;
		rateCurve = "USD-SOFR"; // Default rate curve
        if (underlying.rfind("SGD", 0) == 0 || underlying.rfind("STI",0) == 0) {
            rateCurve = "SGD-SORA";
        }
	}
	inline string getType() const override { return tradeType; }; // Override from Trade
	inline string getUnderlying() const override { return underlying; }; // Override from Trade
	inline double getNotional() const override { return notional; }; // Override from Trade

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
        // For American options, value is max of immediate exercise or holding (continuation)
		return std::max(Payoff(S), continuation);
	}

    // Getters from TreeProduct base
    OptionType getOptType() const override { return optType; }
    double getStrike() const override { return strike; }
    const std::string& getRateCurve() const override { return rateCurve; }


// private: // Changed to protected so derived classes (like AmerCallSpread) can access if needed
protected:
	OptionType optType;
	double strike;
	Date expiryDate;
    std::string rateCurve;
};

// Example for American Spread - similar to EuroSpread but inherits AmericanOption
class AmerCallSpread : public AmericanOption {
public:
	AmerCallSpread(const std::string& name_arg, double notional_arg, double k1, double k2, const Date& start_arg, const Date& expiry_arg)
		// : strike1_val(k1), strike2_val(k2) // Direct member init
	{
        tradeType = "AmerCallSpread";
        underlying = to_upper(name_arg);
        strike = k1; // Set base AmericanOption strike to k1
        strike1_val = k1;
        strike2_val = k2;
        expiryDate = expiry_arg; // Set base AmericanOption expiry
        notional = notional_arg;
        tradeDate = start_arg;
        optType = OptionType::Call; // Base type is Call for a call spread
		rateCurve = "USD-SOFR"; // Default, adjust as needed
        if (underlying.rfind("SGD", 0) == 0 || underlying.rfind("STI",0) == 0) {
             rateCurve = "SGD-SORA";
        }
		assert(k1 < k2);
	};

    // Override Payoff for the spread
	double Payoff(double S) const override
	{
		return PAYOFF::CallSpread(strike1_val, strike2_val, S);
	}
	// GetExpiry, ValueAtNode, getOptType, getStrike, getRateCurve can be inherited if AmericanOption's are suitable.
    // ValueAtNode will correctly use the spread's Payoff due to virtual dispatch on Payoff(S).

private:
	double strike1_val; // Lower strike
	double strike2_val; // Upper strike
    // expiryDate is inherited from AmericanOption protected members
};

#endif
