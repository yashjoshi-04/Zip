#pragma once
#include <iostream>
#include <string>
#include <map>
#include <unordered_map>
#include <future>
#include <memory>
#include <stdexcept>

#include "Trade.h"
#include "Market.h"
#include "Pricer.h"
#include "Date.h"

struct MarketShock {
	std::string market_id;
	Date tenor_date;
	double shock_value;
};

class CurveDecorator : public Market {
public:
	CurveDecorator(const Market& mkt, const MarketShock& curveRateShock)
        : thisMarketUp(mkt), thisMarketDown(mkt)
	{
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
    Market thisMarketUp;
    Market thisMarketDown;
};

class PriceDecorator : public Market {
public:
	PriceDecorator(const Market& mkt, const MarketShock& stockPriceShock)
        : thisMarketUp(mkt), thisMarketDown(mkt)
    {
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
	RiskEngine(const Market& market, double ir_curve_shock_val, double vol_curve_shock_val, double stock_price_shock_val)
        : originalMarket(market)
	{
        MarketShock usd_ir_shock;
        usd_ir_shock.market_id = "USD-SOFR";
        usd_ir_shock.tenor_date = Date();
        usd_ir_shock.shock_value = ir_curve_shock_val;
        if (market.hasCurve("USD-SOFR")) {
		    curveShocks.emplace("USD-SOFR", CurveDecorator(market, usd_ir_shock));
        }

        MarketShock sgd_ir_shock;
        sgd_ir_shock.market_id = "SGD-SORA";
        sgd_ir_shock.tenor_date = Date();
        sgd_ir_shock.shock_value = ir_curve_shock_val;
        if (market.hasCurve("SGD-SORA")) {
		    curveShocks.emplace("SGD-SORA", CurveDecorator(market, sgd_ir_shock));
        }

        MarketShock logvol_shock;
        logvol_shock.market_id = "LOGVOL";
        logvol_shock.tenor_date = Date();
        logvol_shock.shock_value = vol_curve_shock_val;
        if (market.hasVolCurve("LOGVOL")) {
		    volShocks.emplace("LOGVOL", VolDecorator(market, logvol_shock));
        }
	}

	void computeRisk(std::shared_ptr<Trade> trade, const std::string& riskType, std::shared_ptr<Pricer> pricer, bool singleThread);

	inline const std::map<std::string, double>& getResult() const {
		return result;
	}

    double getRiskMeasure(const std::string& key) const {
        auto it = result.find(key);
        if (it != result.end()) {
            return it->second;
        }
        return 0.0;
    }

private:
    Market originalMarket;
	std::unordered_map<std::string, CurveDecorator> curveShocks;
	std::unordered_map<std::string, VolDecorator> volShocks;
	std::unordered_map<std::string, PriceDecorator> priceShocks;
	std::map<std::string, double> result;
};
```

**File 2: `Factory.h`** (Ensuring includes and `std::make_shared` are correct)
