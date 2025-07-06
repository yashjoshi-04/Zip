#pragma once
#include <iostream>
#include <string>
#include <map>
#include <unordered_map>
#include <future>
#include <memory> // For std::shared_ptr

#include "Trade.h"
#include "Market.h"
#include "Pricer.h" // Include Pricer.h

using namespace std;

struct MarketShock {
	string market_id; // e.g., "USD-SOFR", "LOGVOL", "AAPL" (for stock price)
	Date tenor_date;  // Date of the specific tenor to shock (or Date() for parallel shock)
	double shock_value; // e.g., 0.0001 for 1bp, 0.01 for 1% vol
};

class CurveDecorator : public Market {
public:
	CurveDecorator(const Market& mkt, const MarketShock& curveRateShock)
        : thisMarketUp(mkt), thisMarketDown(mkt)
	{
		// cout << "CurveDecorator created for IR curve: " << curveRateShock.market_id << endl;
		auto curve_up = thisMarketUp.getCurve(curveRateShock.market_id);
        if (!curve_up) throw std::runtime_error("IR curve not found for up-shock: " + curveRateShock.market_id);
		curve_up->shock(curveRateShock.tenor_date, curveRateShock.shock_value);

		auto curve_down = thisMarketDown.getCurve(curveRateShock.market_id);
        if (!curve_down) throw std::runtime_error("IR curve not found for down-shock: " + curveRateShock.market_id);
		curve_down->shock(curveRateShock.tenor_date, -1.0 * curveRateShock.shock_value);
	}
	inline const Market& getMarketUp() const { return thisMarketUp; }
	inline const Market& getMarketDown() const { return thisMarketDown; }

private:
	Market thisMarketUp;
	Market thisMarketDown;
};

class VolDecorator : public Market {
public:
    VolDecorator(const Market& mkt, const MarketShock& volCurveShock)
        : thisMarketUp(mkt), thisMarketDown(mkt)
    {
        // cout << "VolDecorator created for vol curve: " << volCurveShock.market_id << endl;
        auto vol_curve_up = thisMarketUp.getVolCurve(volCurveShock.market_id);
        if (!vol_curve_up) throw std::runtime_error("Vol curve not found for up-shock: " + volCurveShock.market_id);
        vol_curve_up->shock(volCurveShock.tenor_date, volCurveShock.shock_value);

        auto vol_curve_down = thisMarketDown.getVolCurve(volCurveShock.market_id);
        if (!vol_curve_down) throw std::runtime_error("Vol curve not found for down-shock: " + volCurveShock.market_id);
        vol_curve_down->shock(volCurveShock.tenor_date, -1.0 * volCurveShock.shock_value);
    }

    inline const Market& getMarketUp() const { return thisMarketUp; }
    inline const Market& getMarketDown() const { return thisMarketDown; }

private:
    Market thisMarketUp;   // For vol bumped up
    Market thisMarketDown; // For vol bumped down
};

class PriceDecorator : public Market { // For stock price delta
public:
	PriceDecorator(const Market& mkt, const MarketShock& stockPriceShock)
        : thisMarketUp(mkt), thisMarketDown(mkt)
    {
		// cout << "PriceDecorator created for stock: " << stockPriceShock.market_id << endl;
        // Assuming shockPrice is on Market class and handles underlying name.
        // Market::shockPrice(const string& underlying, double shock_amount)
		thisMarketUp.shockPrice(stockPriceShock.market_id, stockPriceShock.shock_value);
        thisMarketDown.shockPrice(stockPriceShock.market_id, -1.0 * stockPriceShock.shock_value);
	}

	inline const Market& getMarketUp() const { return thisMarketUp; }
    inline const Market& getMarketDown() const { return thisMarketDown; }
private:
	Market thisMarketUp;
    Market thisMarketDown;
};


class RiskEngine
{
public:
    // Constructor takes the base market and shock magnitudes
	RiskEngine(const Market& market, double ir_curve_shock_val, double vol_curve_shock_val, double stock_price_shock_val)
        : originalMarket(market) // Store original market if needed, or just use for decorators
	{
		// Initialize CurveShocks for relevant IR curves
        // Assuming "USD-SOFR" and "SGD-SORA" are the main ones.
        // Could be made more dynamic if market contains many curves.
        MarketShock usd_ir_shock;
        usd_ir_shock.market_id = "USD-SOFR";
        usd_ir_shock.tenor_date = Date(); // Parallel shock
        usd_ir_shock.shock_value = ir_curve_shock_val; // e.g., 0.0001 for 1bp
        // Check if "USD-SOFR" curve exists in market before creating decorator
        if (market.getCurve("USD-SOFR")) {
		    curveShocks.emplace("USD-SOFR", CurveDecorator(market, usd_ir_shock));
        } else {
            cout << "Warning: IR Curve 'USD-SOFR' not found in market. Skipping shock for it." << endl;
        }

        MarketShock sgd_ir_shock;
        sgd_ir_shock.market_id = "SGD-SORA";
        sgd_ir_shock.tenor_date = Date(); // Parallel shock
        sgd_ir_shock.shock_value = ir_curve_shock_val;
        if (market.getCurve("SGD-SORA")) {
		    curveShocks.emplace("SGD-SORA", CurveDecorator(market, sgd_ir_shock));
        } else {
            cout << "Warning: IR Curve 'SGD-SORA' not found in market. Skipping shock for it." << endl;
        }

		// Initialize VolShocks for relevant Vol curves (e.g., "LOGVOL")
        MarketShock logvol_shock;
        logvol_shock.market_id = "LOGVOL"; // Assuming one main vol curve for options
        logvol_shock.tenor_date = Date(); // Parallel shock
        logvol_shock.shock_value = vol_curve_shock_val; // e.g., 0.01 for 1%
        if (market.getVolCurve("LOGVOL")) {
		    volShocks.emplace("LOGVOL", VolDecorator(market, logvol_shock));
        } else {
            cout << "Warning: Vol Curve 'LOGVOL' not found in market. Skipping shock for it." << endl;
        }

        // Initialize PriceShocks - example for a stock, can be expanded
        // This requires knowing which underlyings to shock.
        // For now, let's assume we might want to shock specific stock prices later if required.
        // Example: if trade is on "APPL"
        // MarketShock appl_stock_shock;
        // appl_stock_shock.market_id = "APPL"; // Stock ticker
        // appl_stock_shock.tenor_date = Date(); // Not used for stock price shock
        // appl_stock_shock.shock_value = stock_price_shock_val; // e.g. 1.0 for $1 shock
        // if (market.getStockPrice("APPL")) { // Need a hasStock method or try-catch getStockPrice
        //     priceShocks.emplace("APPL", PriceDecorator(market, appl_stock_shock));
        // }


		// cout << "Risk engine created." << endl;
	}

    // Compute risk for a given trade, risk type, using a specific pricer (for options)
	void computeRisk(std::shared_ptr<Trade> trade, const std::string& riskType, std::shared_ptr<Pricer> pricer, bool singleThread);

	inline const map<string, double>& getResult() const {
		// cout << "Risk result: " << endl;
		// for (const auto& pair : result) {
        //     std::cout << pair.first << ": " << pair.second << std::endl;
        // }
		return result;
	}

    // Helper to get a specific risk result by key (e.g. "USD-SOFR_DV01" or "LOGVOL_VEGA")
    double getRiskMeasure(const std::string& key) const {
        auto it = result.find(key);
        if (it != result.end()) {
            return it->second;
        }
        return 0.0; // Or throw error if key not found
    }


private:
    Market originalMarket; // Keep a copy of the original market for reference if needed
	unordered_map<string, CurveDecorator> curveShocks;
	unordered_map<string, VolDecorator> volShocks;
	unordered_map<string, PriceDecorator> priceShocks; // For stock price delta

	map<string, double> result; // Stores results like {"USD-SOFR_DV01": value, "LOGVOL_VEGA": value}
};

// Ensure MarketShock struct uses tenor_date and shock_value consistently
// In Market.h: RateCurve::shock(Date tenor, double value), VolCurve::shock(Date tenor, double value)
// Market::shockPrice(const string& underlying, double shock)

// The Date() default constructor in MarketShock implies parallel shock for curves/vols.
// If specific tenor shocks were needed, the Date object would be specific.
// The current shock methods in RateCurve/VolCurve implement parallel shock regardless of tenor Date.
// This is consistent with "parallel shock all tenors rate by 1bp".
// Date() in MarketShock.tenor_date is fine for this.
// shock_value is the bump_amount (e.g. 0.0001 or 0.01)
```
