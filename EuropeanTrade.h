#ifndef _EUROPEAN_TRADE
#define _EUROPEAN_TRADE

#include <cassert>
#include <string> // For std::string
#include "TreeProduct.h"
#include "Payoff.h"
#include "Types.h" // For OptionType

class EuropeanOption : public TreeProduct {
public:
	EuropeanOption() {}; // Default constructor
	EuropeanOption(OptionType optType_arg, double notional_arg, double strike_arg, const Date& start_arg, const Date& expiry_arg, const std::string& name_arg)
	{
		tradeType = "EuropeanOption"; // More specific type
		underlying = to_upper(name_arg); // Assuming to_upper is available (e.g. from helper.h)
		optType = optType_arg;
		strike = strike_arg;
		expiryDate = expiry_arg;
		notional = notional_arg;
		tradeDate = start_arg; // Trade initiation date
		// Default rate curve for discounting, can be changed or made configurable
        // For USD underlyings, USD-SOFR is typical. For others, might need adjustment.
		rateCurve = "USD-SOFR";
        // If underlying is SGD based, could default to "SGD-SORA"
        if (underlying.rfind("SGD", 0) == 0 || underlying.rfind("STI",0) == 0) { // Simple check if underlying starts with SGD or is STI
            rateCurve = "SGD-SORA";
        }
	};
	inline string getType() const override { return tradeType; }; // Override from Trade
	inline string getUnderlying() const override { return underlying; }; // Override from Trade
	inline double getNotional() const override { return notional; }; // Override from Trade

	// TreeProduct interface implementations
	double Payoff(double S) const override { return PAYOFF::VanillaOption(optType, strike, S); }
	const Date& GetExpiry() const override { return expiryDate; }
	double ValueAtNode(double S, double t, double continuation) const override {
        // For European options, value at node is always the continuation value (discounted expected future value)
        // unless it's the terminal node, where it's the payoff.
        // The tree pricer handles terminal payoffs separately.
        return continuation;
    }

    // Getters from TreeProduct base
    OptionType getOptType() const override { return optType; }
    double getStrike() const override { return strike; }
    const std::string& getRateCurve() const override { return rateCurve; }

protected:
	OptionType optType;
	double strike = 0;
	Date expiryDate;
	std::string rateCurve; // Name of the rate curve to use for discounting this option

};

// Example of a more complex European Product, e.g. Call Spread
class EuroCallSpread : public EuropeanOption { // Inherits from EuropeanOption for some base properties
public:
	EuroCallSpread(const std::string& name_arg, double notional_arg, double k1, double k2, const Date& start_arg, const Date& expiry_arg)
     // : strike1(k1), strike2(k2) // Direct member init
    {
        tradeType = "EuroCallSpread";
        underlying = to_upper(name_arg);
        // For a spread, 'strike' of base class might be K1 or not used. Payoff is custom.
        strike = k1; // Set base strike to k1 for convention
        strike1 = k1;
        strike2 = k2;
        expiryDate = expiry_arg; // Set base expiry
        notional = notional_arg;
        tradeDate = start_arg;
        optType = OptionType::Call; // Base type is Call for a call spread
		rateCurve = "USD-SOFR"; // Default, adjust as needed
        if (underlying.rfind("SGD", 0) == 0 || underlying.rfind("STI",0) == 0) {
             rateCurve = "SGD-SORA";
        }
		assert(k1 < k2); // Lower strike must be less than higher strike
	};

    // Override Payoff for the spread
	double Payoff(double S) const override { return PAYOFF::CallSpread(strike1, strike2, S); };
	// GetExpiry, ValueAtNode, getOptType, getStrike, getRateCurve can be inherited if EuropeanOption's are suitable
    // Or override getStrike if it should return something specific for spread (e.g. (k1+k2)/2 or just k1)
    // For now, inherits EuropeanOption's getStrike() which returns `this->strike` (i.e. k1).

private:
	double strike1; // Lower strike
	double strike2; // Upper strike
    // expiryDate is inherited from EuropeanOption protected members
};

#endif
