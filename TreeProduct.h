#ifndef _TREE_PRODUCT_H
#define _TREE_PRODUCT_H
#include "Date.h"
#include "Trade.h"   // Ensure Trade is fully defined
#include "Types.h"   // For OptionType
#include <string>    // For std::string

class TreeProduct: public Trade
{
public:
    TreeProduct() { tradeType = "TreeProduct";} // Constructor sets tradeType
    virtual ~TreeProduct() = default; // Add virtual destructor

    // Pure virtual functions from Trade base class (if any) must be implemented or also pure here
    // string getType() const override { return tradeType; } // Already handled by specific option type
    // string getUnderlying() const override { return underlying; }
    // double getNotional() const override { return notional; }

    // TreeProduct specific interface
    virtual const Date& GetExpiry() const = 0;
    virtual double ValueAtNode(double stockPrice, double t, double continuationValue) const = 0;

    // Common getters for options, needed by Pricer/Main
    virtual OptionType getOptType() const = 0;
    virtual double getStrike() const = 0;
    virtual const std::string& getRateCurve() const = 0;

    // Pv method for TreeProduct is usually handled by a Pricer.
    // Override from Trade, but indicate it's not the primary pricing path.
    double Pv(const Market& mkt) const override {
        // This Pv should not be called directly for options if using a Pricer.
        // Pricer::Price will dispatch to Pricer::PriceTree.
        // throw std::runtime_error("TreeProduct::Pv should not be called directly. Use a Pricer.");
        return 0.0; // Placeholder, consistent with original
    };
};

#endif
